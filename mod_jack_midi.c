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

#include <jack/midiport.h>

extern jack_nframes_t jack_ntp_desync(app_t *app, osc_time_t tstamp);

static int
_process(jack_nframes_t nframes, void *data)
{
	slave_t *slave = data;
	jack_ringbuffer_t *rb = slave->rb;
	jack_midi_event_t jev;
	
	jack_nframes_t last = jack_last_frame_time(slave->app->client);

	void *port_buf = jack_port_get_buffer(slave->port, nframes);
	jack_midi_clear_buffer(port_buf);

	while(jack_ringbuffer_read_space(rb) >= sizeof(jack_midi_event_t))
	{
		if(jack_ringbuffer_peek(rb, (char *)&jev, sizeof(jack_midi_event_t)) 
			== sizeof(jack_midi_event_t))
		{
			if(jev.time >= last + nframes)
				break;

			if(jev.time == 0)
				jev.time = last;
			else if(jev.time < last)
			{
				fprintf(stderr, "[mod_jack_midi] late event: -%i\n", last - jev.time);
				jev.time = last;
			}

			if(jack_ringbuffer_read_space(rb) >= sizeof(jack_midi_event_t) + jev.size)
			{
				if(jack_midi_max_event_size(port_buf) >= jev.size)
				{
					jack_ringbuffer_read_advance(rb, sizeof(jack_midi_event_t));

					uint8_t *m = jack_midi_event_reserve(port_buf, jev.time-last, jev.size);
					if(m)
						jack_ringbuffer_read(rb, (char *)m, jev.size);
					else
					{
						fprintf(stderr, "[mod_jack_midi] could not reserve event\n");
						jack_ringbuffer_read_advance(rb, jev.size);
					}
				}
			}
		}
	}

	return 0;
}

static int
_call(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	slave_t *slave = luaL_checkudata(L, 1, "jack_midi_t");
	jack_ringbuffer_t *rb = slave->rb;
	
	osc_time_t tstamp = luaL_checknumber(L, 2);

	jack_midi_event_t jev;

	jev.time = jack_ntp_desync(app, tstamp);
	jev.size = lua_gettop(L) - 2;

	if(jack_ringbuffer_write_space(rb) >= sizeof(jack_midi_event_t) + jev.size)
	{
		if(jack_ringbuffer_write(rb, (const char *)&jev, sizeof(jack_midi_event_t))
			== sizeof(jack_midi_event_t))
		{
			for(int i=0; i<jev.size; i++)
			{
				uint8_t m = luaL_checkint(L, i+3);
				if(jack_ringbuffer_write(rb, (const char *)&m, sizeof(uint8_t)) != sizeof(uint8_t))
					break; //FIXME throw and handle error
			}
		}
	}

	return 0;
}

static int
_gc(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	slave_t *slave = luaL_checkudata(L, 1, "jack_midi_t");

	if(slave->rb)
		jack_ringbuffer_free(slave->rb);
	slave->rb = NULL;

	if(slave->port)
		jack_port_unregister(app->client, slave->port);
	slave->port = NULL;

	if(jack_ringbuffer_write_space(app->rb) >= sizeof(job_t))
	{
		job_t job = {
			.add = 0,
			.slave = slave
		};

		if(jack_ringbuffer_write(app->rb, (const char *)&job, sizeof(job_t))
				!= sizeof(job_t))
			fprintf(stderr, "[mod_jack_midi] [jack] ringbuffer write error\n");
	}
	else
		fprintf(stderr, "[mod_jack_midi] [jack] ringbuffer overflow\n");

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
	const char *port = luaL_optstring(L, -1, "midi_out");
	lua_pop(L, 1);

	slave->app = app;
	slave->process = _process;
	slave->data = slave;
	if(!(slave->port = jack_port_register(app->client, port,
			JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0)))
		goto fail;
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
			fprintf(stderr, "[mod_jack_midi] [jack] ringbuffer write error\n");
	}
	else
		fprintf(stderr, "[mod_jack_midi] [jack] ringbuffer overflow\n");

	luaL_getmetatable(L, "jack_midi_t");
	lua_setmetatable(L, -2);

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg ljack_midi [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_jack_midi(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "jack_midi_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, litem, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "JACK_MIDI", ljack_midi, 1);

	return 1;
}
