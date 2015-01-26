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

uint8_t *
zip_read(app_t *app, const char *key, size_t *size)
{
	struct zip_stat stat;
	if(!zip_stat(app->io, key, 0, &stat))
	{
		size_t fsize = stat.size;
		struct zip_file *f = zip_fopen(app->io, key, 0);
		if(f)
		{
			uint8_t *str = rt_alloc(app, fsize);
			if(str)
			{
				if(zip_fread(f, str, fsize) == -1)
				{
					rt_free(app, str);
					str = NULL;
					fsize = 0;
				}
				else
					; //success
			}
			zip_fclose(f);

			*size = fsize;
			return str;
		}
	}

	*size = 0;
	return NULL;
}

static int
_call(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));

	const char *key = luaL_checkstring(L, 1);

	size_t size;
	uint8_t *chunk = zip_read(app, key, &size);

	if(chunk)
	{
		lua_pushlstring(L, (char *)chunk, size);
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
