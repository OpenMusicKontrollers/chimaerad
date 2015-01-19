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

char *
zip_read(app_t *app, const char *key, size_t *size)
{
	char *str = NULL;
	struct zip_file *f = NULL;
	struct zip_stat stat;
	size_t fsize;

	if(zip_stat(app->io, key, 0, &stat))
		goto fail;
	fsize = stat.size;

	f = zip_fopen(app->io, key, 0);
	if(!f)
		goto fail;
	
	str = rt_alloc(app, fsize + 1);
	if(!str)
		goto fail;

	if(zip_fread(f, str, fsize) == -1)
		goto fail;
	zip_fclose(f);
	str[fsize] = 0;

	*size = fsize;
	return str;

fail:
	if(str)
		rt_free(app, str);
	if(f)
		zip_fclose(f);

	*size = 0;
	return NULL;
}

static int
_call(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));

	const char *key = luaL_checkstring(L, 1);

	size_t size;
	char *chunk = zip_read(app, key, &size);

	if(chunk)
	{
		lua_pushlstring(L, chunk, size);
		rt_free(app, chunk);
	}
	else
		lua_pushnil(L);

	return 1;
}

static const luaL_Reg lzip [] = {
	{"read", _call},
	{NULL, NULL}
};

int
luaopen_zip(app_t *app)
{
	lua_State *L = app->L;

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "ZIP", lzip, 1);		

	return 1;
}
