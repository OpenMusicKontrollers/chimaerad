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

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#include <uv.h>

#include <varchunk.h>

#include <osc.h>
#include <osc_stream.h>
#include <mod_osc_common.h>

#define BUF_SIZE 0x10000
#define CHUNK_SIZE 0x1000

typedef struct _mod_osc_t mod_osc_t;
typedef struct _mod_msg_t mod_msg_t;

struct _mod_osc_t {
	lua_State *L;
	osc_stream_t *stream;
	osc_time_t time;
	varchunk_t *from_net;
	varchunk_t *to_net;
};

static int
_call(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	mod_osc_t *mod_osc = luaL_checkudata(L, 1, "mod_osc_t");

	if(lua_gettop(L) < 4)
		return 0;

	osc_data_t *buf = varchunk_write_request(mod_osc->to_net, CHUNK_SIZE);
	if(buf)
	{
		osc_data_t *ptr = buf;
		osc_data_t *end = buf + CHUNK_SIZE;

		//osc_time_t tstamp = luaL_checknumber(L, 2); //TODO
		ptr = mod_osc_encode(L, 3, ptr, end);

		size_t len = ptr - buf;
		if(len && osc_check_message(buf, len))
		{
			varchunk_write_advance(mod_osc->to_net, len);
			osc_stream_flush(mod_osc->stream);
		}
	}

	return 0;
}

static int
_gc(lua_State *L)
{
	mod_osc_t *mod_osc = luaL_checkudata(L, 1, "mod_osc_t");

	if(!mod_osc)
		return 0;

	osc_stream_free(mod_osc->stream);
	varchunk_free(mod_osc->from_net);
	varchunk_free(mod_osc->to_net);
	mod_osc->L = NULL;
	
	lua_pushlightuserdata(L, mod_osc);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	return 0;
}

static const luaL_Reg lmt [] = {
	{"__call", _call},
	{"__gc", _gc},
	{"close", _gc},
	{NULL, NULL}
};

static void
_stamp(osc_time_t tstamp, void *data)
{
	mod_osc_t *mod_osc = data;
	lua_State *L = mod_osc->L;

	mod_osc->time = tstamp;
}

static void
_message(const osc_data_t *buf, size_t len, void *data)
{
	mod_osc_t *mod_osc = data;
	lua_State *L = mod_osc->L;

	const osc_data_t *ptr = buf;
	const char *path;
	const char *fmt;
			
	lua_pushlightuserdata(L, mod_osc);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if(!lua_isnil(L, -1))
	{
		ptr = osc_get_path(ptr, &path);
		ptr = osc_get_fmt(ptr, &fmt);
		fmt++;

		lua_pushnumber(L, mod_osc->time);
		lua_pushstring(L, path);
		lua_pushstring(L, fmt);

		for(const char *type = fmt; *type; type++)
			switch(*type)
			{
				case OSC_INT32:
				{
					int32_t i;
					ptr = osc_get_int32(ptr, &i);
					lua_pushnumber(L, i);
					break;
				}
				case OSC_FLOAT:
				{
					float f;
					ptr = osc_get_float(ptr, &f);
					lua_pushnumber(L, f);
					break;
				}
				case OSC_STRING:
				{
					const char *s;
					ptr = osc_get_string(ptr, &s);
					lua_pushstring(L, s);
					break;
				}
				case OSC_BLOB:
				{
					osc_blob_t b;
					ptr = osc_get_blob(ptr, &b);
					mod_blob_t *tb = lua_newuserdata(L, sizeof(mod_blob_t) + b.size);
					if(tb)
					{
						tb->size = b.size;
						memcpy(tb->buf, b.payload, b.size);
						
						luaL_getmetatable(L, "mod_blob_t");
						lua_setmetatable(L, -2);
					}
					else
						lua_pushnil(L);
					break;
				}
				
				case OSC_TIMETAG:
				{
					osc_time_t t;
					ptr = osc_get_timetag(ptr, &t);
					lua_pushnumber(L, t);
					break;
				}
				case OSC_INT64:
				{
					int64_t h;
					ptr = osc_get_int64(ptr, &h);
					lua_pushnumber(L, h);
					break;
				}
				case OSC_DOUBLE:
				{
					double d;
					ptr = osc_get_double(ptr, &d);
					lua_pushnumber(L, d);
					break;
				}
				
				case OSC_NIL:
				case OSC_BANG:
				case OSC_TRUE:
				case OSC_FALSE:
					break;
				
				case OSC_SYMBOL:
				{
					const char *S;
					ptr = osc_get_symbol(ptr, &S);
					lua_pushstring(L, S);
					break;
				}
				case OSC_CHAR:
				{
					char c;
					ptr = osc_get_char(ptr, &c);
					lua_pushnumber(L, c);
					break;
				}
				case OSC_MIDI:
				{
					const uint8_t *m;
					ptr = osc_get_midi(ptr, &m);
					lua_createtable(L, 4, 0);
					lua_pushnumber(L, m[0]);
					lua_rawseti(L, -2, 1);
					lua_pushnumber(L, m[1]);
					lua_rawseti(L, -2, 2);
					lua_pushnumber(L, m[2]);
					lua_rawseti(L, -2, 3);
					lua_pushnumber(L, m[3]);
					lua_rawseti(L, -2, 4);
					break;
				}
			}

		if(lua_pcall(L, 3+strlen(fmt), 0, 0))
		{
			fprintf(stderr, "_message: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1); // pop error string
		}
	}
	else
		lua_pop(L, 1);
		
	lua_gc(L, LUA_GCSTEP, 0);
}

static const osc_unroll_inject_t inject = {
	.stamp = _stamp,
	.message = _message,
	.bundle = NULL
};

static void *
_data_recv_req(size_t size, void *data)
{
	mod_osc_t *mod_osc = data;

	return varchunk_write_request(mod_osc->from_net, size);
}

static void
_data_recv_adv(size_t written, void *data)
{
	mod_osc_t *mod_osc = data;

	varchunk_write_advance(mod_osc->from_net, written);

	const osc_data_t *ptr;
	size_t size;
	while((ptr = varchunk_read_request(mod_osc->from_net, &size)))
	{
		if(!osc_unroll_packet((osc_data_t *)ptr, size, OSC_UNROLL_MODE_FULL, (osc_unroll_inject_t *)&inject, mod_osc))
			fprintf(stderr, "invalid OSC packet\n");

		varchunk_read_advance(mod_osc->from_net);		
	}
}

static const void *
_data_send_req(size_t *len, void *data)
{
	mod_osc_t *mod_osc = data;

	return varchunk_read_request(mod_osc->to_net, len);
}

static void
_data_send_adv(void *data)
{
	mod_osc_t *mod_osc = data;

	varchunk_read_advance(mod_osc->to_net);
}

static void
_data_free(void *data)
{
	mod_osc_t *mod_osc = data;

	mod_osc->stream = NULL;
}

static const osc_stream_driver_t driver = {
	.recv_req = _data_recv_req,
	.recv_adv = _data_recv_adv,
	.send_req = _data_send_req,
	.send_adv = _data_send_adv,
	.free = _data_free
};

static int
_new(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	const char *url = luaL_checkstring(L, 1);

	mod_osc_t *mod_osc = lua_newuserdata(L, sizeof(mod_osc_t));
	if(!mod_osc)
		goto fail;
	memset(mod_osc, 0, sizeof(mod_osc_t));
	mod_osc->L = L;

	if(!(mod_osc->stream = osc_stream_new(app->loop, url, &driver, mod_osc)))
		goto fail;
	
	if(!(mod_osc->from_net = varchunk_new(BUF_SIZE)))
		goto fail;
	if(!(mod_osc->to_net = varchunk_new(BUF_SIZE)))
		goto fail;

	luaL_getmetatable(L, "mod_osc_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, mod_osc);
	lua_pushvalue(L, 2); // push callback
	lua_rawset(L, LUA_REGISTRYINDEX);

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static int
_blob(lua_State *L)
{
	int size = luaL_checkinteger(L, 1);
	mod_blob_t *tb = lua_newuserdata(L, sizeof(mod_blob_t) + size);
	if(!tb)
		goto fail;
	tb->size = size;
	memset(tb->buf, 0, size);
	
	luaL_getmetatable(L, "mod_blob_t");
	lua_setmetatable(L, -2);

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg losc [] = {
	{"new", _new},
	{"blob", _blob},
	{NULL, NULL}
};

static int
_blob_index(lua_State *L)
{
	mod_blob_t *tb = luaL_checkudata(L, 1, "mod_blob_t");

	int typ = lua_type(L, 2);
	if(typ == LUA_TNUMBER)
	{
		int index = luaL_checkinteger(L, 2);
		if( (index >= 0) && (index < tb->size) )
			lua_pushnumber(L, tb->buf[index]);
		else
			lua_pushnil(L);
	}
	else if( (typ == LUA_TSTRING) && !strcmp(lua_tostring(L, 2), "raw") )
		lua_pushlightuserdata(L, tb->buf);
	else
		lua_pushnil(L);

	return 1;
}

static int
_blob_newindex(lua_State *L)
{
	mod_blob_t *tb = luaL_checkudata(L, 1, "mod_blob_t");
	int index = luaL_checkinteger(L, 2);

	if( (index >= 0) && (index < tb->size) )
		tb->buf[index] = luaL_checkinteger(L, 3);

	return 0;
}

static int
_blob_len(lua_State *L)
{
	mod_blob_t *tb = luaL_checkudata(L, 1, "mod_blob_t");

	lua_pushnumber(L, tb->size);

	return 1;
}

static const luaL_Reg lblob [] = {
	{"__index", _blob_index},
	{"__newindex", _blob_newindex},
	{"__len", _blob_len},
	{NULL, NULL}
};

int
luaopen_osc(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "mod_osc_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, lmt, 1);
	lua_pop(L, 1);
	
	luaL_newmetatable(L, "mod_blob_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, lblob, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "OSC", losc, 1);

	return 1;
}
