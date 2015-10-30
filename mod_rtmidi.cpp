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

#include <cstring>

#include <chimaerad.h>

extern "C"
{
#define LUA_COMPAT_MODULE
#include <lauxlib.h>
#include <lualib.h>
}

#include <RtMidi.h>

typedef struct _mod_rtmidi_out_t mod_rtmidi_out_t;
//typedef struct _mod_rtmidi_in_t mod_rtmidi_in_t;

struct _mod_rtmidi_out_t {
	RtMidiOut *io;
};

//struct _mod_rtmidi_in_t {
//	RtMidiIn *io;
//};

static int
_out_open_port(lua_State *L)
{
	mod_rtmidi_out_t *mod_rtmidi = (mod_rtmidi_out_t *)luaL_checkudata(L, 1, "mod_rtmidi_out_t");

  try
	{
		mod_rtmidi->io->openPort(0, "Output");
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
	}

	return 0;
}

static int
_out_open_virtual_port(lua_State *L)
{
	mod_rtmidi_out_t *mod_rtmidi = (mod_rtmidi_out_t *)luaL_checkudata(L, 1, "mod_rtmidi_out_t");

  try
	{
		mod_rtmidi->io->openVirtualPort("VirtualOutput");
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
	}

	return 0;
}

static int
_out_close_port(lua_State *L)
{
	mod_rtmidi_out_t *mod_rtmidi = (mod_rtmidi_out_t *)luaL_checkudata(L, 1, "mod_rtmidi_out_t");

  try
	{
		mod_rtmidi->io->closePort();
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
	}

	return 0;
}

static int
_out_ports(lua_State *L)
{
	mod_rtmidi_out_t *mod_rtmidi = (mod_rtmidi_out_t *)luaL_checkudata(L, 1, "mod_rtmidi_out_t");

  try
	{
		unsigned int count = mod_rtmidi->io->getPortCount();

		lua_createtable(L, count, 0);
		for(unsigned int i=0; i<count; i++)
		{
			const char *name = mod_rtmidi->io->getPortName(i).c_str();
			lua_pushstring(L, name);
			lua_rawseti(L, -2, i+1);
		}
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
		lua_pushnil(L);
	}

	return 1;
}

static int
_out_send(lua_State *L)
{
	mod_rtmidi_out_t *mod_rtmidi = (mod_rtmidi_out_t *)luaL_checkudata(L, 1, "mod_rtmidi_out_t");

	//int time = luaL_checkinteger(L, 2);
	int n = lua_gettop(L) - 2;
	std::vector<unsigned char> msg(n);
	for(int i=0; i<n; i++)
		msg[i] = luaL_checkinteger(L, i+3);

  try
	{
		mod_rtmidi->io->sendMessage(&msg);
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
	}

	return 0;
}

static int
_out_gc(lua_State *L)
{
	mod_rtmidi_out_t *mod_rtmidi = (mod_rtmidi_out_t *)luaL_checkudata(L, 1, "mod_rtmidi_out_t");

  try
	{
		mod_rtmidi->io->closePort();
		delete mod_rtmidi->io;
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
	}

	return 0;
}

static const luaL_Reg lmt_out [] = {
	{"open", _out_open_port},
	{"open_virtual", _out_open_virtual_port},
	{"close", _out_close_port},

	{"ports", _out_ports},
	
	{"__call", _out_send},
	{"__gc", _out_gc},
	{NULL, NULL}
};

static int
_new(lua_State *L)
{
	mod_rtmidi_out_t *mod_rtmidi = (mod_rtmidi_out_t *)lua_newuserdata(L, sizeof(mod_rtmidi_out_t));
	memset(mod_rtmidi, 0, sizeof(mod_rtmidi_out_t));

	RtMidi::Api api = RtMidi::UNSPECIFIED;	
	if(lua_gettop(L) > 0)
	{
		const char *str = luaL_checkstring(L, 1);
		if(!strcmp(str, "UNSPECIFIED"))
			api = RtMidi::UNSPECIFIED;
		else if(!strcmp(str, "MACOSX_CORE"))
			api = RtMidi::MACOSX_CORE;
		else if(!strcmp(str, "LINUX_ALSA"))
			api = RtMidi::LINUX_ALSA;
		else if(!strcmp(str, "UNIX_JACK"))
			api = RtMidi::UNIX_JACK;
		else if(!strcmp(str, "WINDOWS_MM"))
			api = RtMidi::WINDOWS_MM;
		else if(!strcmp(str, "RTMIDI_DUMMY"))
			api = RtMidi::RTMIDI_DUMMY;
	}
	
  try
	{
		mod_rtmidi->io = new RtMidiOut(api, "ChimaeraD");
		luaL_getmetatable(L, "mod_rtmidi_out_t");
		lua_setmetatable(L, -2);
  }
	catch(RtMidiError &error)
	{
		error.printMessage();
		lua_pop(L, 1);
		lua_pushnil(L);
	}

	return 1;
}

static int
_apis(lua_State *L)
{
	lua_newtable(L);

	try
	{
		std::vector<RtMidi::Api> apis;
		RtMidi::getCompiledApi(apis);
	
		for(std::vector<RtMidi::Api>::size_type i = 0; i != apis.size(); i++)
		{
			switch(apis[i])
			{
				case RtMidi::UNSPECIFIED:
					lua_pushstring(L, "UNSPECIFIED");
					lua_rawseti(L, -2, i+1);
					break;
				case RtMidi::MACOSX_CORE:
					lua_pushstring(L, "MACOSX_CORE");
					lua_rawseti(L, -2, i+1);
					break;
				case RtMidi::LINUX_ALSA:
					lua_pushstring(L, "LINUX_ALSA");
					lua_rawseti(L, -2, i+1);
					break;
				case RtMidi::UNIX_JACK:
					lua_pushstring(L, "UNIX_JACK");
					lua_rawseti(L, -2, i+1);
					break;
				case RtMidi::WINDOWS_MM:
					lua_pushstring(L, "WINDOWS_MM");
					lua_rawseti(L, -2, i+1);
					break;
				case RtMidi::RTMIDI_DUMMY:
					lua_pushstring(L, "RTMIDI_DUMMY");
					lua_rawseti(L, -2, i+1);
					break;
			}
		}
	}
	catch(RtMidiError &error)
	{
		error.printMessage();
	}

	return 1;

  //enum Api {
  //  UNSPECIFIED,    /*!< Search for a working compiled API. */
  //  MACOSX_CORE,    /*!< Macintosh OS-X Core Midi API. */
  //  LINUX_ALSA,     /*!< The Advanced Linux Sound Architecture API. */
  //  UNIX_JACK,      /*!< The JACK Low-Latency MIDI Server API. */
  //  WINDOWS_MM,     /*!< The Microsoft Multimedia MIDI API. */
  //  RTMIDI_DUMMY    /*!< A compilable but non-functional API. */
  //};
}

static const luaL_Reg lrtmidi [] = {
	{"new", _new},
	{"apis", _apis},
	{NULL, NULL}
};

extern "C"
{
int
luaopen_rtmidi(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "mod_rtmidi_out_t");
	luaL_openlib(L, NULL, lmt_out, 0);
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "RTMIDI", lrtmidi, 1);

	return 1;
}
}
