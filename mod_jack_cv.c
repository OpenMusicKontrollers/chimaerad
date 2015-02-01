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

#	if defined(JACK_HAS_METADATA_API)
#	include <jackey.h>
#	include <jack/metadata.h>
#	include <jack/uuid.h>
#	endif

typedef struct _cv_event_t cv_event_t;

struct _cv_event_t {
	jack_nframes_t time;
	jack_default_audio_sample_t sample;
};

static int
_process(jack_nframes_t nframes, void *data)
{
	slave_t *slave = data;
	jack_ringbuffer_t *rb = slave->rb;
	cv_event_t cev;

	jack_default_audio_sample_t *port_buf = jack_port_get_buffer(slave->port, nframes);
	jack_nframes_t i = 0;
	jack_default_audio_sample_t sample = slave->sample;

	while(jack_ringbuffer_read_space(rb) >= sizeof(cv_event_t))
	{
		if(jack_ringbuffer_read(rb, (char *)&cev, sizeof(cv_event_t)) 
			== sizeof(cv_event_t))
		{
			for( ; i<cev.time; i++)
				port_buf[i] = sample;
			sample = cev.sample;
		}
		else
			; //FIXME report error
	}

	for( ; i<nframes; i++)
		port_buf[i] = sample;

	slave->sample = sample;

	return 0;
}

static int
_call(lua_State *L)
{
	slave_t *slave = luaL_checkudata(L, 1, "jack_cv_t");
	jack_ringbuffer_t *rb = slave->rb;

	cv_event_t cev;

	cev.time = luaL_checkint(L, 2);
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
	if(!(slave->rb = jack_ringbuffer_create(4096)))
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
