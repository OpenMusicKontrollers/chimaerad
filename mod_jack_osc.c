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
#include <jack_osc.h>
#include <mod_osc_common.h>

#	if defined(JACK_HAS_METADATA_API)
#	include <jackey.h>
#	include <jack/metadata.h>
#	include <jack/uuid.h>
#	endif

extern jack_nframes_t jack_ntp_desync(app_t *app, osc_time_t tstamp);

typedef struct _osc_event_t osc_event_t;

struct _osc_event_t
{
	INLIST;

	jack_nframes_t time;
	size_t size;
	osc_data_t buf [0];
};

static int
_sort(const void *data1, const void *data2)
{
	const osc_event_t *oev1 = data1;
	const osc_event_t *oev2 = data2;

	return oev1->time <= oev2->time ? -1 : 1;
}

static int
_process(jack_nframes_t nframes, void *data)
{
	slave_t *slave = data;
	app_t *app = slave->app;
	jack_ringbuffer_t *rb = slave->rb;
	jack_osc_event_t jev;
	osc_event_t *oev;
	Inlist *l;

	if(!slave->port || !slave->rb)
		return 0;
	
	jack_nframes_t last = jack_last_frame_time(app->client);

	void *port_buf = jack_port_get_buffer(slave->port, nframes);
	jack_osc_clear_buffer(port_buf);

	// drain ringbuffer and add to timely sorted list
	while(jack_ringbuffer_read_space(rb) >= sizeof(jack_osc_event_t))
	{
		if(jack_ringbuffer_peek(rb, (char *)&jev, sizeof(jack_osc_event_t)) ==
			sizeof(jack_osc_event_t))
		{
			if(jack_ringbuffer_read_space(rb) >= sizeof(jack_osc_event_t) + jev.size)
			{
				jack_ringbuffer_read_advance(rb, sizeof(jack_osc_event_t));

				oev = rt_alloc(app, sizeof(osc_event_t) + jev.size);
				if(oev)
				{
					oev->time = jev.time;
					oev->size = jev.size;
					jack_ringbuffer_read(rb, (char *)oev->buf, jev.size); //TODO check

					slave->messages = inlist_sorted_insert(slave->messages, INLIST_GET(oev),
						_sort);
				}
				else
				{
					rt_printf(app, "[mod_jack_osc] out of memory\n");
					jack_ringbuffer_read_advance(rb, jev.size);
				}
			}
		}
	}

	// dispatch messages scheduled for this cycle
	INLIST_FOREACH_SAFE(slave->messages, l, oev)
	{
		if(oev->time >= last + nframes)
			break; // done for this cycle
		else if(oev->time == 0) // immediate execution
			oev->time = last;
		else if(oev->time < last)
		{
			rt_printf(app, "[mod_jack_osc] late event: -%i\n", last - oev->time);
			oev->time = last;
		}

		if(jack_osc_max_event_size(port_buf) >= oev->size)
			jack_osc_event_write(port_buf, oev->time-last, oev->buf, oev->size);
		else
			rt_printf(app, "[mod_jack_osc] osc buffer overflow\n");


		slave->messages = inlist_remove(slave->messages, INLIST_GET(oev));
		rt_free(app, oev);
	}

	return 0;
}

static int
_call(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	slave_t *slave = luaL_checkudata(L, 1, "jack_osc_t");
	jack_ringbuffer_t *rb = slave->rb;

	osc_data_t buf [2048];
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + 2048;
	
	osc_time_t tstamp = luaL_checknumber(L, 2);

	jack_osc_event_t jev;

	jev.time = jack_ntp_desync(app, tstamp);
	ptr = mod_osc_encode(L, 3, ptr, end);

	if(ptr)
	{
		jev.size = ptr - buf;

		if(jack_ringbuffer_write_space(rb) >= sizeof(jack_osc_event_t) + jev.size)
		{
			if(jack_ringbuffer_write(rb, (const char *)&jev, sizeof(jack_osc_event_t))
				== sizeof(jack_osc_event_t))
			{
				if(jack_ringbuffer_write(rb, (const char *)&buf, jev.size) != jev.size)
					fprintf(stderr, "[mod_jack_osc] [jack] ringbuffer write error\n");
			}
		}
		else
			fprintf(stderr, "[mod_jack_osc] [jack] ringbuffer overflow\n");
	}

	return 0;
}

static int
_gc(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	slave_t *slave = luaL_checkudata(L, 1, "jack_osc_t");

	if(slave->rb)
		jack_ringbuffer_free(slave->rb);
	slave->rb = NULL;

	if(slave->port)
	{
#if defined(JACK_HAS_METADATA_API)
		jack_uuid_t uuid = jack_port_uuid(slave->port);
		jack_remove_property(app->client, uuid, JACKEY_EVENT_TYPES);
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
			fprintf(stderr, "[mod_jack_osc] [jack] ringbuffer write error\n");
	}
	else
		fprintf(stderr, "[mod_jack_osc] [jack] ringbuffer overflow\n");

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
	const char *port = luaL_optstring(L, -1, "osc_out");
	lua_pop(L, 1);

	slave->app = app;
	slave->process = _process;
	slave->data = slave;
	if(!(slave->port = jack_port_register(app->client, port,
			JACK_DEFAULT_OSC_TYPE, JackPortIsOutput, 0)))
		goto fail;
#if defined(JACK_HAS_METADATA_API)
	jack_uuid_t uuid = jack_port_uuid(slave->port);
	if(jack_set_property(app->client, uuid, JACKEY_EVENT_TYPES,
			"OSC", "text/plain"))
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
			fprintf(stderr, "[mod_jack_osc] [jack] ringbuffer write error\n");
	}
	else
		fprintf(stderr, "[mod_jack_osc] [jack] ringbuffer overflow\n");

	luaL_getmetatable(L, "jack_osc_t");
	lua_setmetatable(L, -2);

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg ljack_osc [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_jack_osc(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "jack_osc_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, litem, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "JACK_OSC", ljack_osc, 1);

	return 1;
}
