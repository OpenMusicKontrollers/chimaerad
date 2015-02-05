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

typedef struct _midi_event_t midi_event_t;

struct _midi_event_t
{
	INLIST;

	jack_nframes_t time;
	size_t size;
	jack_midi_data_t buf [0];
};

static int
_sort(const void *data1, const void *data2)
{
	const midi_event_t *mev1 = data1;
	const midi_event_t *mev2 = data2;

	return mev1->time <= mev2->time ? -1 : 1;
}

static int
_process(jack_nframes_t nframes, void *data)
{
	slave_t *slave = data;
	app_t *app = slave->app;
	jack_ringbuffer_t *rb = slave->rb;
	jack_midi_event_t jev;
	midi_event_t *mev;
	Inlist *l;

	if(!slave->port || !slave->rb)
		return 0;
	
	jack_nframes_t last = jack_last_frame_time(app->client);

	void *port_buf = jack_port_get_buffer(slave->port, nframes);
	jack_midi_clear_buffer(port_buf);

	// drain ringbuffer and add to timely sorted list
	while(jack_ringbuffer_read_space(rb) >= sizeof(jack_midi_event_t))
	{
		if(jack_ringbuffer_peek(rb, (char *)&jev, sizeof(jack_midi_event_t)) ==
			sizeof(jack_midi_event_t))
		{
			if(jack_ringbuffer_read_space(rb) >= sizeof(jack_midi_event_t) + jev.size)
			{
				jack_ringbuffer_read_advance(rb, sizeof(jack_midi_event_t));

				mev = rt_alloc(app, sizeof(midi_event_t) + jev.size);
				if(mev)
				{
					mev->time = jev.time;
					mev->size = jev.size;
					jack_ringbuffer_read(rb, (char *)mev->buf, jev.size); //TODO check

					slave->messages = inlist_sorted_insert(slave->messages, INLIST_GET(mev),
						_sort);
				}
				else
				{
					rt_printf(app, "[mod_jack_midi] out of memory\n");
					jack_ringbuffer_read_advance(rb, jev.size);
				}
			}
		}
	}

	// dispatch messages scheduled for this cycle
	INLIST_FOREACH_SAFE(slave->messages, l, mev)
	{
		if(mev->time >= last + nframes)
			break; // done for this cycle
		else if(mev->time == 0) // immediate execution
			mev->time = last;
		else if(mev->time < last)
		{
			rt_printf(app, "[mod_jack_midi] late event: -%i\n", last - mev->time);
			mev->time = last;
		}

		if(jack_midi_max_event_size(port_buf) >= mev->size)
			jack_midi_event_write(port_buf, mev->time-last, mev->buf, mev->size);
		else
			rt_printf(app, "[mod_jack_midi] midi buffer overflow\n");


		slave->messages = inlist_remove(slave->messages, INLIST_GET(mev));
		rt_free(app, mev);
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
