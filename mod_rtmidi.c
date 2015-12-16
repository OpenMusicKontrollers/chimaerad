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

#include <stdlib.h>
#include <string.h>

#include <chimaerad.h>

#include <lua.h>
#include <lauxlib.h>

#include <rtmidi_c.h>

typedef struct _mod_midi_out_t mod_midi_out_t;

struct _mod_midi_out_t {
	int has_core;
	int has_alsa;
	int has_jack;
	int has_mm;

	unsigned port_number;
	const char *port_name;
	RtMidiC_Out *mout;
};

static int
_call(lua_State *L)
{
	mod_midi_out_t *mod_midi_out = luaL_checkudata(L, 1, "mod_midi_out_t");

	const unsigned len = lua_gettop(L) - 1;
	uint8_t buf [2048]; //FIXME check len <= 2048

	for(unsigned i = 0; i < len; i++)
		buf[i] = luaL_checkinteger(L, 2+i);

	if(rtmidic_out_send_message(mod_midi_out->mout, len, buf))
		lua_pushboolean(L, 0); // fail
	else
		lua_pushboolean(L, 1); // success

	return 1;
}

static int
_gc(lua_State *L)
{
	mod_midi_out_t *mod_midi_out = luaL_checkudata(L, 1, "mod_midi_out_t");

	rtmidic_out_port_close(mod_midi_out->mout);
	rtmidic_out_free(mod_midi_out->mout);

	return 0;
}

static int
_list(lua_State *L)
{
	mod_midi_out_t *mod_midi_out = luaL_checkudata(L, 1, "mod_midi_out_t");

	lua_newtable(L);

	const unsigned port_count = rtmidic_out_port_count(mod_midi_out->mout);

	for(unsigned i = 0, pos = 1; i < port_count; i++)
	{
		const char *name = rtmidic_out_port_name(mod_midi_out->mout, i);
		if(name)
		{
			lua_pushstring(L, name);
			lua_rawseti(L, -2, pos++);
		}
	}

	return 1;
}

static int
_backend(lua_State *L)
{
	mod_midi_out_t *mod_midi_out = luaL_checkudata(L, 1, "mod_midi_out_t");

	lua_newtable(L);

	unsigned pos = 1;

	if(mod_midi_out->has_core)
	{
		lua_pushstring(L, "MACOSX_CORE");
		lua_rawseti(L, -2, pos++);
	}
	if(mod_midi_out->has_alsa)
	{
		lua_pushstring(L, "LINUX_ALSA");
		lua_rawseti(L, -2, pos++);
	}
	if(mod_midi_out->has_jack)
	{
		lua_pushstring(L, "UNIX_JACK");
		lua_rawseti(L, -2, pos++);
	}
	if(mod_midi_out->has_mm)
	{
		lua_pushstring(L, "WINDOWS_MM");
		lua_rawseti(L, -2, pos++);
	}

	return 1;
}

static int
_virtual_port_open(lua_State *L)
{
	mod_midi_out_t *mod_midi_out = luaL_checkudata(L, 1, "mod_midi_out_t");
	mod_midi_out->port_name = luaL_checkstring(L, 2);

	if(rtmidic_out_virtual_port_open(mod_midi_out->mout, mod_midi_out->port_name))
		lua_pushboolean(L, 0); // fail
	else
		lua_pushboolean(L, 1); // success

	return 1;
}

static int
_port_open(lua_State *L)
{
	mod_midi_out_t *mod_midi_out = luaL_checkudata(L, 1, "mod_midi_out_t");
	mod_midi_out->port_number = luaL_checkinteger(L, 2);
	mod_midi_out->port_name = luaL_checkstring(L, 3);

	if(rtmidic_out_port_open(mod_midi_out->mout, mod_midi_out->port_number, mod_midi_out->port_name))
		lua_pushboolean(L, 0); // fail
	else
		lua_pushboolean(L, 1); // success

	return 1;
}

static int
_port_close(lua_State *L)
{
	mod_midi_out_t *mod_midi_out = luaL_checkudata(L, 1, "mod_midi_out_t");

	if(rtmidic_out_port_close(mod_midi_out->mout))
		lua_pushboolean(L, 0); // fail
	else
		lua_pushboolean(L, 1); // success

	return 1;
}

static const luaL_Reg lmt_out [] = {
	{"__call", _call},
	{"__gc", _gc},
	{"list", _list},
	{"backend", _backend},
	{"port_open", _port_open},
	{"virtual_port_open", _virtual_port_open},
	{"port_close", _port_close},

	{NULL, NULL}
};

static int
_new(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	const char *client_name = luaL_optstring(L, 1, "ChimaeraD");

	mod_midi_out_t *mod_midi_out = lua_newuserdata(L, sizeof(mod_midi_out_t));
	if(!mod_midi_out)
		goto fail;
	memset(mod_midi_out, 0, sizeof(mod_midi_out_t));

	mod_midi_out->mout = rtmidic_out_new(RTMIDIC_API_UNSPECIFIED, client_name);

	mod_midi_out->has_core = rtmidic_has_compiled_api(RTMIDIC_API_MACOSX_CORE);
	mod_midi_out->has_alsa = rtmidic_has_compiled_api(RTMIDIC_API_LINUX_ALSA);
	mod_midi_out->has_jack = rtmidic_has_compiled_api(RTMIDIC_API_UNIX_JACK);
	mod_midi_out->has_mm = rtmidic_has_compiled_api(RTMIDIC_API_WINDOWS_MM);
	
	luaL_getmetatable(L, "mod_midi_out_t");
	lua_setmetatable(L, -2);

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg lrtmidi [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_rtmidi(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "mod_midi_out_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_setfuncs(L, lmt_out, 1);
	lua_pop(L, 1);

	lua_newtable(L);
	lua_pushlightuserdata(L, app);
	luaL_setfuncs(L, lrtmidi, 1);
	lua_setglobal(L, "RTMIDI");

	return 0;
}
