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

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) && !defined(__WINDOWS__)
#	define ZIP_EXTERN __declspec(dllexport) // needed for static linking with mingw-w64
#endif
#include <zip.h>

extern void * rt_alloc(size_t len);
extern void * rt_realloc(size_t len, void *buf);
extern void rt_free(void *buf);

typedef struct _mod_zip_t mod_zip_t;

struct _mod_zip_t {
	struct zip *io;
};

static int
_read(lua_State *L)
{
	mod_zip_t *mod_zip = luaL_checkudata(L, 1, "mod_zip_t");
	const char *path = luaL_checkstring(L, 2);

	struct zip_stat stat;
	if(zip_stat(mod_zip->io, path, 0, &stat))
		goto err;
	size_t fsize = stat.size;

	struct zip_file *f = zip_fopen(mod_zip->io, path, 0);
	if(!f)
		goto err;
	
	char *str = rt_alloc(fsize + 1);
	if(!str)
		goto err;

	if(zip_fread(f, str, fsize) == -1)
		goto err;
	zip_fclose(f);
	str[fsize] = 0;

	lua_pushlstring(L, str, fsize);
	return 1;

err:
	if(str)
		rt_free(str);
	if(f)
		zip_fclose(f);

	lua_pushnil(L);
	return 1;
}

static int
_gc(lua_State *L)
{
	mod_zip_t *mod_zip = luaL_checkudata(L, 1, "mod_zip_t");

	zip_close(mod_zip->io);

	return 0;
}

static const luaL_Reg lmt [] = {
	{"__call", _read},
	{"__gc", _gc},
	{NULL, NULL}
};

static int
_new(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	mod_zip_t *mod_zip = lua_newuserdata(L, sizeof(mod_zip_t));
	memset(mod_zip, 0, sizeof(mod_zip_t));

	int err;
	mod_zip->io = zip_open(path, ZIP_CHECKCONS, &err);
	if(!mod_zip->io)
	{
		fprintf(stderr, "zip_open: %i\n", err);
		lua_pushnil(L);
	}
	else
	{
		luaL_getmetatable(L, "mod_zip_t");
		lua_setmetatable(L, -2);
	}

	return 1;
}

static const luaL_Reg lzip [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_zip(lua_State *L)
{
	luaL_newmetatable(L, "mod_zip_t");
	luaL_register(L, NULL, lmt);
	lua_pop(L, 1);

	luaL_register(L, "ZIP", lzip);		

	return 1;
}
