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

#include <chimaerad.h>

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#if(LUA_VERSION_NUM >= 502)
#	define lua_objlen lua_rawlen
#endif

#include <cJSON.h>

static app_t *app_ptr; //FIXME try to circumvent static global

// forward declaration
static cJSON * _encode_item(lua_State *L, int idx);

static cJSON *
_encode_array(lua_State *L, int idx)
{
	cJSON *arr = cJSON_CreateArray();
	idx = idx < 0 ? idx-1 : idx;

	lua_pushnil(L);
	while(lua_next(L, idx))
	{
		int pos = luaL_checkinteger(L, -2);
		cJSON *item = _encode_item(L, -1);
		if(item)
			cJSON_AddItemToArray(arr, item);
		else
			fprintf(stderr, " at %i\n", pos);

		lua_pop(L, 1);
	}

	return arr;
}

static cJSON *
_encode_object(lua_State *L, int idx)
{
	cJSON *obj = cJSON_CreateObject();
	idx = idx < 0 ? idx-1 : idx;

	lua_pushnil(L);
	while(lua_next(L, idx))
	{
		const char *name = luaL_checkstring(L, -2);
		cJSON *item = _encode_item(L, -1);
		if(item)
			cJSON_AddItemToObject(obj, name, item);
		else
			fprintf(stderr, " at %s\n", name);

		lua_pop(L, 1);
	}

	return obj;
}

static cJSON *
_encode_item(lua_State *L, int idx)
{
	switch(lua_type(L, idx))
	{
		case LUA_TNIL:
			return cJSON_CreateNull();
		case LUA_TBOOLEAN:
			return cJSON_CreateBool(lua_toboolean(L, idx));
		case LUA_TNUMBER:
			return cJSON_CreateNumber(luaL_checknumber(L, idx));
		case LUA_TSTRING:
			return cJSON_CreateString(luaL_checkstring(L, idx));
		case LUA_TTABLE:
			if(!lua_objlen(L, idx))
				return _encode_object(L, idx);
			else
				return _encode_array(L, idx);
		default:
			fprintf(stderr, "cannot encode %s", lua_typename(L, idx));
			return NULL;
	}
}

// Lua -> JSON
static int
_encode(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));

	if(lua_istable(L, 1) && !lua_objlen(L, 1))
	{
		cJSON *root = _encode_object(L, 1);
		char *json = cJSON_PrintUnformatted(root);
		lua_pushnil(L); // no error
		lua_pushstring(L, json);
		rt_free(app, json);
		cJSON_Delete(root);
	}
	else
	{
		lua_pushstring(L, "not an object table");
		lua_pushnil(L);
	}

	return 2;
}

// forward declaration
static void _decode_item(lua_State *L, cJSON *item);

static void
_decode_array(lua_State *L, cJSON *arr)
{
	int n = cJSON_GetArraySize(arr);
	lua_createtable(L, n, 0);

	int i = 0;
	for(cJSON *item = arr->child; item; item = item->next)
	{
		_decode_item(L, item);
		lua_rawseti(L, -2, ++i);
	}
}

static void
_decode_object(lua_State *L, cJSON *obj)
{
	int n = cJSON_GetArraySize(obj);
	lua_createtable(L, 0, n);

	for(cJSON *item = obj->child; item; item = item->next)
	{
		_decode_item(L, item);
		lua_setfield(L, -2, item->string);
	}
}

static void
_decode_item(lua_State *L, cJSON *item)
{
	switch(item->type)
	{
		case cJSON_False:
			lua_pushboolean(L, 0);
			break;
		case cJSON_True:
			lua_pushboolean(L, 1);
			break;
		case cJSON_NULL:
			lua_pushnil(L);
			break;
		case cJSON_Number:
			lua_pushnumber(L, item->valuedouble);
			break;
		case cJSON_String:
			lua_pushstring(L, item->valuestring);
			break;
		case cJSON_Array:
			_decode_array(L, item);
			break;
		case cJSON_Object:
			_decode_object(L, item);
			break;
	}
}

// JSON -> Lua
static int
_decode(lua_State *L)
{
	const char *root_str = luaL_checkstring(L, 1);

	cJSON *root = cJSON_Parse(root_str);
	if(root)
	{
		lua_pushnil(L); // no error
		_decode_object(L, root);
		cJSON_Delete(root);
	}
	else
	{
		const char *err = cJSON_GetErrorPtr();
		lua_pushstring(L, err);
		lua_pushnil(L);
	}

	return 2;
}

static const luaL_Reg ljson [] = {
	{"encode", _encode},
	{"decode", _decode},
	{NULL, NULL}
};

static void *
_malloc_fn(size_t size)
{
	return rt_alloc(app_ptr, size);
}

static void
_free_fn(void *ptr)
{
	rt_free(app_ptr, ptr);
}

int
luaopen_json(app_t *app)
{
	lua_State *L = app->L;

	app_ptr = app;
	cJSON_Hooks hooks = {
		.malloc_fn = _malloc_fn,
		.free_fn = _free_fn
	};
	cJSON_InitHooks(&hooks);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "JSON", ljson, 1);

	return 1;
}
