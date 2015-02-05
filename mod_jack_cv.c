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

#include <string.h>

#include <chimaerad.h>

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#include <osc.h>

#	if defined(JACK_HAS_METADATA_API)
#	include <jackey.h>
#	include <jack/metadata.h>
#	include <jack/uuid.h>
#	endif

extern jack_nframes_t jack_ntp_desync(app_t *app, osc_time_t tstamp);

typedef struct _cv_event_t cv_event_t;

struct _cv_event_t {
	INLIST;

	jack_nframes_t time;
	jack_default_audio_sample_t sample;
};

static int
_sort(const void *data1, const void *data2)
{
	const cv_event_t *cev1 = data1;
	const cv_event_t *cev2 = data2;

	return cev1->time <= cev2->time ? -1 : 1;
}

static int
_process(jack_nframes_t nframes, void *data)
{
	slave_t *slave = data;
	app_t *app = slave->app;
	jack_ringbuffer_t *rb = slave->rb;
	cv_event_t *cev;
	Inlist *l;

	if(!slave->port || !slave->rb)
		return 0;

	jack_nframes_t last = jack_last_frame_time(app->client);

	jack_default_audio_sample_t *port_buf = jack_port_get_buffer(slave->port, nframes);
	jack_nframes_t i = 0;
	jack_default_audio_sample_t sample = slave->sample;

	// drain ringbuffer and add to timely sorted list
	while(jack_ringbuffer_read_space(rb) >= sizeof(cv_event_t))
	{
		cev = rt_alloc(app, sizeof(cv_event_t));
		if(cev)
		{
			if(jack_ringbuffer_read(rb, (char *)cev, sizeof(cv_event_t)) ==
				sizeof(cv_event_t))
			{
				slave->messages = inlist_sorted_insert(slave->messages, INLIST_GET(cev),
					_sort);
			}
			else
			{
				rt_printf(app, "[mod_jack_cv] ringbuffer read error\n");
				rt_free(app, cev);
			}
		}
		else
		{
			rt_printf(app, "[mod_jack_cv] out of memory\n");
			jack_ringbuffer_read_advance(rb, sizeof(cv_event_t));
		}
	}

	// dispatch messages scheduled for this cycle
	INLIST_FOREACH_SAFE(slave->messages, l, cev)
	{
		if(cev->time >= last + nframes)
			break; // done for this cycle
		else if(cev->time == 0) // immediate execution
			cev->time = last;
		else if(cev->time < last)
		{
			rt_printf(app, "[mod_jack_cv] late event: -%i\n", last - cev->time);
			cev->time = last;
		}

		for( ; i<cev->time - last; i++)
			port_buf[i] = sample;
		sample = cev->sample;

		slave->messages = inlist_remove(slave->messages, INLIST_GET(cev));
		rt_free(app, cev);
	}

	for( ; i<nframes; i++)
		port_buf[i] = sample;

	slave->sample = sample;

	return 0;
}

static int
_call(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	slave_t *slave = luaL_checkudata(L, 1, "jack_cv_t");
	jack_ringbuffer_t *rb = slave->rb;
	
	osc_time_t tstamp = luaL_checknumber(L, 2);

	cv_event_t cev;
	cev.time = jack_ntp_desync(app, tstamp);
	cev.sample = luaL_checknumber(L, 3);

	if(jack_ringbuffer_write_space(rb) >= sizeof(cv_event_t))
	{
		if(jack_ringbuffer_write(rb, (const char *)&cev, sizeof(cv_event_t))
				!= sizeof(cv_event_t))
			fprintf(stderr, "[mod_jack_cv] [jack] ringbuffer wite error\n");
	}
	else
		fprintf(stderr, "[mod_jack_cv] [jack] ringbuffer overflow\n");

	return 0;
}

static int
_gc(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	slave_t *slave = luaL_checkudata(L, 1, "jack_cv_t");

	if(slave->rb)
		jack_ringbuffer_free(slave->rb);
	slave->rb = NULL;

	if(slave->port)
	{
#if defined(JACK_HAS_METADATA_API)
		jack_uuid_t uuid = jack_port_uuid(slave->port);
		jack_remove_property(app->client, uuid, JACKEY_SIGNAL_TYPE);
#endif
		jack_port_unregister(app->client, slave->port);
	}
	slave->port = NULL;

	if(jack_ringbuffer_write_space(app->rb) >= sizeof(job_t))
	{
		job_t job = {
			.add = 0,
			.slave = slave
		};

		if(jack_ringbuffer_write(app->rb, (const char *)&job, sizeof(job_t))
				!= sizeof(job_t))
			fprintf(stderr, "[mod_jack_cv] [jack] ringbuffer write error\n");
	}
	else
		fprintf(stderr, "[mod_jack_cv] [jack] ringbuffer overflow\n");

	return 0;
}

static const luaL_Reg litem [] = {
	{"__call", _call},
	{"__gc", _gc},
	{"close", _gc},
	{NULL, NULL}
};

static int
_new(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	slave_t *slave = lua_newuserdata(L, sizeof(slave_t));
	if(!slave)
		goto fail;
	memset(slave, 0, sizeof(slave_t));

	lua_getfield(L, 1, "port");
	const char *port = luaL_optstring(L, -1, "cv_out");
	lua_pop(L, 1);

	slave->app = app;
	slave->process = _process;
	slave->data = slave;
	if(!(slave->port = jack_port_register(app->client, port,
			JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0)))
		goto fail;
#if defined(JACK_HAS_METADATA_API)
	jack_uuid_t uuid = jack_port_uuid(slave->port);
	if(jack_set_property(app->client, uuid,
			JACKEY_SIGNAL_TYPE, "CV", "text/plain"))
		goto fail;
#endif
	if(!(slave->rb = jack_ringbuffer_create(0x8000)))
		goto fail;

	if(jack_ringbuffer_write_space(app->rb) >= sizeof(job_t))
	{
		job_t job = {
			.add = 1,
			.slave = slave
		};

		if(jack_ringbuffer_write(app->rb, (const char *)&job, sizeof(job_t))
				!= sizeof(job_t))
			fprintf(stderr, "[mod_jack_cv] [jack] ringbuffer write error\n");
	}
	else
		fprintf(stderr, "[mod_jack_cv] [jack] ringbuffer overflow\n");

	luaL_getmetatable(L, "jack_cv_t");
	lua_setmetatable(L, -2);

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg ljack_cv [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_jack_cv(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "jack_cv_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, litem, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "JACK_CV", ljack_cv, 1);

	return 1;
}
