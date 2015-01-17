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

#include <inlist.h>

#include <osc.h>
#include <osc_stream.h>

typedef struct _mod_osc_t mod_osc_t;
typedef struct _mod_msg_t mod_msg_t;
typedef struct _mod_blob_t mod_blob_t;

struct _mod_osc_t {
	lua_State *L;
	osc_stream_t stream;
	Inlist *messages;
};

struct _mod_msg_t {
	INLIST;
	app_t *app;
	osc_data_t buf [OSC_STREAM_BUF_SIZE];
	size_t len;
};

struct _mod_blob_t {
	int32_t size;
	uint8_t buf [0];
};

static void
_trigger(mod_osc_t *mod_osc)
{
	mod_msg_t *msg = INLIST_CONTAINER_GET(mod_osc->messages, mod_msg_t);

	if(osc_check_message(msg->buf, msg->len))
		osc_stream_send(&mod_osc->stream, msg->buf, msg->len);
}

static void
_on_sent(osc_stream_t *stream, size_t len, void *data)
{
	mod_osc_t *mod_osc = data;
	lua_State *L = mod_osc->L;

	mod_msg_t *msg = INLIST_CONTAINER_GET(mod_osc->messages, mod_msg_t);
	mod_osc->messages = inlist_remove(mod_osc->messages, mod_osc->messages);
	rt_free(msg->app, msg);

	if(mod_osc->messages)
		_trigger(mod_osc);
}

static int
_send(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	mod_osc_t *mod_osc = luaL_checkudata(L, 1, "mod_osc_t");

	if(lua_gettop(L) < 4)
		return 0;

	osc_time_t tstamp = luaL_checknumber(L, 2);
	const char *path = luaL_checkstring(L, 3);
	const char *fmt = luaL_checkstring(L, 4);

	if(!path || !fmt)
		return 0;

	mod_msg_t *msg = rt_alloc(app, sizeof(mod_msg_t));
	if(!msg)
		return 0;
	msg->app = app;	

	osc_data_t *buf = msg->buf;
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + OSC_STREAM_BUF_SIZE;

	ptr = osc_set_path(ptr, end, path);
	ptr = osc_set_fmt(ptr, end, fmt);

	int pos = 5;
	for(const char *type = fmt; *type; type++)
		switch(*type)
		{
			case OSC_INT32:
			{
				int32_t i = luaL_checkint(L, pos++);
				ptr = osc_set_int32(ptr, end, i);
				break;
			}
			case OSC_FLOAT:
			{
				float f = luaL_checknumber(L, pos++);
				ptr = osc_set_float(ptr, end, f);
				break;
			}
			case OSC_STRING:
			{
				const char *s = luaL_checkstring(L, pos++);
				ptr = osc_set_string(ptr, end, s);
				break;
			}
			case OSC_BLOB:
			{
				mod_blob_t *tb = luaL_checkudata(L, pos++, "mod_blob_t");
				ptr = osc_set_blob(ptr, end, tb->size, tb->buf);
				break;
			}
			
			case OSC_TIMETAG:
			{
				osc_time_t t = luaL_checknumber(L, pos++);
				ptr = osc_set_timetag(ptr, end, t);
				break;
			}
			case OSC_INT64:
			{
				int64_t h = luaL_checknumber(L, pos++);
				ptr = osc_set_int64(ptr, end, h);
				break;
			}
			case OSC_DOUBLE:
			{
				double d = luaL_checknumber(L, pos++);
				ptr = osc_set_double(ptr, end, d);
				break;
			}
			
			case OSC_NIL:
			case OSC_BANG:
			case OSC_TRUE:
			case OSC_FALSE:
				break;
			
			case OSC_SYMBOL:
			{
				const char *S = luaL_checkstring(L, pos++);
				ptr = osc_set_symbol(ptr, end, S);
				break;
			}
			case OSC_CHAR:
			{
				char c = luaL_checkint(L, pos++);
				ptr = osc_set_char(ptr, end, c);
				break;
			}
			case OSC_MIDI:
			{
				uint8_t *m;
				ptr = osc_set_midi_inline(ptr, end, &m);
				if(lua_istable(L, pos))
				{
					lua_rawgeti(L, pos, 4);
					lua_rawgeti(L, pos, 3);
					lua_rawgeti(L, pos, 2);
					lua_rawgeti(L, pos, 1);
					m[0] = luaL_checkint(L, -1);
					m[1] = luaL_checkint(L, -2);
					m[2] = luaL_checkint(L, -3);
					m[3] = luaL_checkint(L, -4);
					lua_pop(L, 4);
				}
				else
				{
					memset(m, 0, 4);
				}
				pos++;
				break;
			}
		}

	int is_empty = mod_osc->messages == NULL ? 1 : 0;

	if(ptr)
	{
		msg->len = ptr - buf;
		if(msg->len > 0)
			mod_osc->messages = inlist_append(mod_osc->messages, INLIST_GET(msg));
		else
			rt_free(msg->app, msg);
	}

	if(is_empty)
		_trigger(mod_osc);

	return 0;
}

static int
_gc(lua_State *L)
{
	mod_osc_t *mod_osc = luaL_checkudata(L, 1, "mod_osc_t");

	if(!mod_osc)
		return 0;

	osc_stream_deinit(&mod_osc->stream);
	mod_osc->L = NULL;

	lua_pushlightuserdata(L, mod_osc);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	return 0;
}

static const luaL_Reg lmt [] = {
	{"__call", _send},
	{"__gc", _gc},
	{NULL, NULL}
};

static void
_stamp(osc_time_t tstamp, void *data)
{
	mod_osc_t *mod_osc = data;
	lua_State *L = mod_osc->L;

	//TODO
}

static void
_message(osc_data_t *buf, size_t len, void *data)
{
	mod_osc_t *mod_osc = data;
	lua_State *L = mod_osc->L;

	osc_data_t *ptr = buf;
	const char *path;
	const char *fmt;
			
	lua_pushlightuserdata(L, mod_osc);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if(!lua_isnil(L, -1))
	{
		ptr = osc_get_path(ptr, &path);
		ptr = osc_get_fmt(ptr, &fmt);
		fmt++;

		lua_pushnumber(L, 0); //TODO timestamp
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
					uint8_t *m;
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

static void
_on_recv(osc_stream_t *stream, osc_data_t *buf, size_t len, void *data)
{
	mod_osc_t *mod_osc = data;

	if(!osc_unroll_packet(buf, len, OSC_UNROLL_MODE_FULL, (osc_unroll_inject_t *)&inject, mod_osc))
		fprintf(stderr, "invalid OSC packet\n");
}

static int
_new(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	const char *url = luaL_checkstring(L, 1);
	int has_callback = lua_gettop(L) > 1; //TODO check whether this is callable

	mod_osc_t *mod_osc = lua_newuserdata(L, sizeof(mod_osc_t));
	if(!mod_osc)
		goto fail;
	memset(mod_osc, 0, sizeof(mod_osc_t));
	mod_osc->L = L;

	if(osc_stream_init(app->loop, &mod_osc->stream, url, _on_recv, _on_sent, mod_osc))
		goto fail;

	luaL_getmetatable(L, "mod_osc_t");
	lua_setmetatable(L, -2);

	if(has_callback)
	{
		lua_pushlightuserdata(L, mod_osc);
		lua_pushvalue(L, 2); // push callback
		lua_rawset(L, LUA_REGISTRYINDEX);
	}

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static int
_blob(lua_State *L)
{
	int size = luaL_checkint(L, 1);
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
		int index = luaL_checkint(L, 2);
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
	int index = luaL_checkint(L, 2);

	if( (index >= 0) && (index < tb->size) )
		tb->buf[index] = luaL_checkint(L, 3);

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
	lua_State *L = L;

	luaL_newmetatable(L, "mod_osc_t");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, lmt, 1);
	lua_pop(L, 1);
	
	luaL_newmetatable(L, "mod_blob_t");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, lblob, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "OSC", losc, 1);

	return 1;
}
