/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

#include <stdio.h>
#include <string.h>

// include main header
#include <chimaerad.h>

// include Lua
#define LUA_COMPAT_MODULE
#include <lualib.h>
#include <lauxlib.h>

#include <portable_endian.h>

#if defined(__WINDOWS__)
#	include <avrt.h>
#else
#	include <sys/mman.h>
#	include <sched.h>
#	include <pthread.h>
#	if defined(__APPLE__)
#		include <mach/thread_policy.h>
#		include <mach/thread_act.h>
#		include <CoreAudio/HostTime.h>
#	endif
#endif

#define AREA_SIZE 0x2000000UL // 32MB TODO increase dynamically

static void *
_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	app_t *app = ud;
	(void)osize;

	if(nsize == 0) {
		if(ptr)
			tlsf_free(app->rtmem.tlsf, ptr);
		return NULL;
	}
	else {
		if(ptr)
			return tlsf_realloc(app->rtmem.tlsf, ptr, nsize);
		else
			return tlsf_malloc(app->rtmem.tlsf, nsize);
	}
}

static int
_thread_rtprio(int priority)
{
#if defined(__WINDOWS__)
	int mcss_sched_priority;
	mcss_sched_priority = priority > 50 ? AVRT_PRIORITY_CRITICAL : (priority > 0 ? AVRT_PRIORITY_HIGH : AVRT_PRIORITY_NORMAL); // TODO when to use CRITICAL?

	// Multimedia Class Scheduler Service
	DWORD dummy = 0;
	HANDLE task = AvSetMmThreadCharacteristics("Pro Audio", &dummy);
	if(!task)
		fprintf(stderr, "AvSetMmThreadCharacteristics error: %d\n", GetLastError());
	else if(!AvSetMmThreadPriority(task, mcss_sched_priority))
		fprintf(stderr, "AvSetMmThreadPriority error: %d\n", GetLastError());

#else
	pthread_t self = pthread_self();

	struct sched_param schedp;
	memset(&schedp, 0, sizeof(struct sched_param));
	schedp.sched_priority = priority;
	
	if(schedp.sched_priority)
	{
		if(pthread_setschedparam(self, SCHED_RR, &schedp))
			fprintf(stderr, "pthread_setschedparam error\n");
	}

	/* //FIXME
#	if defined(__APPLE__)
	thread_time_constraint_policy_data_t ttcpolicy;
	thread_port_t threadport = pthread_mach_thread_np(pthread_self());

	uint64_t period = 333333; // 1 s / 48000 * 16 
	uint64_t computation = 100000; // 0.1 ms = 100000 ns
	uint64_t constraint = 500000; // 0.5 ms  = 500000 ns
	ttcpolicy.period = AudioConvertNanosToHost(period);
	ttcpolicy.computation = AudioConvertNanosToHost(computation);
	ttcpolicy.constraint = AudioConvertNanosToHost(constraint);
	ttcpolicy.preemptible = 1;

	if(thread_policy_set(threadport, THREAD_TIME_CONSTRAINT_POLICY, (thread_policy_t)&ttcpolicy, THREAD_TIME_CONSTRAINT_COUNT) != KERN_SUCCESS)
		fprintf(stderr, "thread_policy_set failed.\n");
#	endif
	*/
#endif

	return 0;
}

static void
_deinit(app_t *app)
{
	lua_close(app->L);

	uv_signal_stop(&app->sigint);
	uv_signal_stop(&app->sigterm);
#if defined(SIGQUIT)
	uv_signal_stop(&app->sigquit);
#endif

#if defined(USE_JACK)
	if(uv_is_active((uv_handle_t *)&app->syncer))
		uv_timer_stop(&app->syncer);
#endif
}

static void
_sig(uv_signal_t *handle, int signum)
{
	app_t *app = handle->data;

	_deinit(app);
}

int
_zip_loader(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	const char *module = luaL_checkstring(L, 1);

	char key [256];
	sprintf(key, "%s.lua", module);

	size_t size;
	uint8_t *chunk = zip_read(app, key, &size);
	if(chunk)
	{
		//printf("_zip_loader: %s %zu\n", key, size);
		luaL_loadbuffer(L, (char *)chunk, size, module);
		rt_free(app, chunk);
		return 1;
	}

	return 0;
}

#if defined(USE_JACK)
#	define JAN_1970 (uint32_t)0x83aa7e80
#	include <osc.h>

static void
_jack_ntp_sync(uv_timer_t *handle)
{
	app_t *app = handle->data;

	app->t0 = app->t1;

	app->t1 = jack_get_time() / 2;
	clock_gettime(CLOCK_REALTIME, &app->ntp);
	app->t1 += jack_get_time() / 2;
	
	app->ntp.tv_sec += JAN_1970;
	app->t1 += 500000; // ideal period of syncer

	app->T = (app->t1 - app->t0) / 0.5; // estimated us/s
}

jack_nframes_t
jack_ntp_desync(app_t *app, osc_time_t tstamp)
{
	double diff; // time difference of OSC timestamp to current wall clock time (s)

	if(tstamp == OSC_IMMEDIATE)
		return 0; // inject at beginning of next period

	uint32_t tstamp_sec = tstamp >> 32;
	uint32_t tstamp_frac = tstamp & 0xffffffff;

	diff = tstamp_sec;
	diff -= app->ntp.tv_sec;
	diff += tstamp_frac * SLICE;
	diff -= app->ntp.tv_nsec * 1e-9;

	jack_time_t t = app->t0 + diff * app->T;
	return jack_time_to_frames(app->client, t);
}

static int
_process(jack_nframes_t nframes, void *data)
{
	app_t *app = data;
	jack_ringbuffer_t *rb = app->rb;

	// append or remove slaves
	while(jack_ringbuffer_read_space(rb) >= sizeof(job_t))
	{
		job_t job;
		if(jack_ringbuffer_read(rb, (char *)&job, sizeof(job_t)) == sizeof(job_t))
		{
			if(job.add)
				app->slaves = inlist_append(app->slaves, INLIST_GET(job.slave));
			else // !job.add
				app->slaves = inlist_remove(app->slaves, INLIST_GET(job.slave));
		}
	}

	// loop over registered slaves
	slave_t *slave;
	INLIST_FOREACH(app->slaves, slave)
		if(slave->process)
			slave->process(nframes, slave->data);

	return 0;
}
#endif

int
main(int argc, char **argv)
{
	static app_t app;
	int err;

#if defined(__WINDOWS__)
	app.rtmem.area = malloc(AREA_SIZE);
#elif defined(__linux__) || defined(__CYGWIN__)
	app.rtmem.area = mmap(NULL, AREA_SIZE, PROT_READ|PROT_WRITE, MAP_32BIT|MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);
#elif defined(__APPLE__)
	app.rtmem.area = mmap(NULL, AREA_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
#endif
	app.rtmem.tlsf = tlsf_create_with_pool(app.rtmem.area, AREA_SIZE);
	app.rtmem.pool = tlsf_get_pool(app.rtmem.tlsf);

	// use default uv_loop
	app.loop = uv_default_loop();

#if defined(USE_JACK)
	const char *id = "ChimaeraD"; //FIXME add NSM support
	const char *server_name = NULL; //FIXME read from command line
	jack_options_t options = server_name ? JackNullOption | JackServerName : JackNullOption;
	jack_status_t status;
	if(!(app.client = jack_client_open(id, options, &status, server_name)))
		fprintf(stderr, "[main] [jack] could not open client\n");
	if(jack_set_process_callback(app.client, _process, &app))
		fprintf(stderr, "[main] [jack] could not set process callback\n");
	if(!(app.rb = jack_ringbuffer_create(256)))
		fprintf(stderr, "[main] [jack] could not create ringbuffer\n");

	app.syncer.data = &app;
	if((err = uv_timer_init(app.loop, &app.syncer)))
		fprintf(stderr, "[main] [uv_timer_init]: %s\n", uv_err_name(err));
	if((err = uv_timer_start(&app.syncer, _jack_ntp_sync, 0, 500))) // ms
		fprintf(stderr, "[main] [uv_timer_start]: %s\n", uv_err_name(err));
#endif

#if defined(USE_LUAJIT)
	app.L = luaL_newstate(); // use LuaJIT internal memory allocator
#else // Lua 5.1 or 5.2
	app.L = lua_newstate(_lua_alloc, &app); // use TLSF memory allocator
#endif

	luaL_openlibs(app.L);
	luaopen_json(&app);
	luaopen_osc(&app);
	luaopen_http(&app);
	luaopen_zip(&app);
	luaopen_rtmidi(&app);
	luaopen_iface(&app);
	luaopen_dns_sd(&app);
#if defined(USE_JACK)
	luaopen_jack_midi(&app);
	luaopen_jack_osc(&app);
	luaopen_jack_cv(&app);
#endif
	lua_pop(app.L, 7);
	lua_gc(app.L, LUA_GCSTOP, 0); // switch to manual garbage collection

	app.io = zip_open(argv[1], ZIP_CHECKCONS, &err);
	if(!app.io)
		fprintf(stderr, "zip_open: %i\n", err);
	
	// overwrite loader functions with our own FIXME only works for LuaJIT & Lua5.1
	lua_getglobal(app.L, "package");
	if(lua_istable(app.L, -1))
	{
		lua_newtable(app.L);
		{
			lua_pushlightuserdata(app.L, &app);
			lua_pushcclosure(app.L, _zip_loader, 1);
			lua_rawseti(app.L, -2, 1);
		}
#if LUA_VERSION_NUM >= 502
		lua_setfield(app.L, -2, "searchers");
#else
		lua_setfield(app.L, -2, "loaders");
#endif
	}
	lua_pop(app.L, 1); // package
	
	if(luaL_dostring(app.L, "_main = require('main')"))
	{
		fprintf(stderr, "main: %s\n", lua_tostring(app.L, -1));
		lua_pop(app.L, 1);
	}
#if defined(USE_JACK)
	if(jack_activate(app.client))
		fprintf(stderr, "[main] [jack] could not activate jack client\n");
#endif
	
	app.sigint.data = &app;
	if((err = uv_signal_init(app.loop, &app.sigint)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&app.sigint, _sig, SIGINT)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));

	app.sigterm.data = &app;
	if((err = uv_signal_init(app.loop, &app.sigterm)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&app.sigterm, _sig, SIGTERM)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));

#if defined(SIGQUIT)
	app.sigquit.data = &app;
	if((err = uv_signal_init(app.loop, &app.sigquit)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&app.sigquit, _sig, SIGQUIT)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
#endif

	_thread_rtprio(60); //TODO make this configurable
	uv_run(app.loop, UV_RUN_DEFAULT);

#if defined(USE_JACK)
	if(app.rb)
		jack_ringbuffer_free(app.rb);

	if(app.client) {
		jack_deactivate(app.client);
		jack_client_close(app.client);
	}
#endif

	if(app.io)
		zip_close(app.io);
	
	tlsf_remove_pool(app.rtmem.tlsf, app.rtmem.pool);
	tlsf_destroy(app.rtmem.tlsf);
#if defined(__WINDOWS__)
	free(app.rtmem.area);
#else
	munmap(app.rtmem.area, AREA_SIZE);
#endif

	return 0;
}
