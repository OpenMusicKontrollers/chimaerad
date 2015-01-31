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
#include <stdint.h>

#include <chimaerad.h>

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#include <uv.h>

#include <inlist.h>

static const luaL_Reg litem [] = {
	{NULL, NULL}
};

static int
_new(lua_State *L)
{
	slave_t *slave = lua_newuserdata(L, sizeof(slave_t));

	//TODO

	return 1;
}

static const luaL_Reg ljack_cv [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_jack_cv(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "jack_cv_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, litem, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "JACK_CV", ljack_cv, 1);

	return 1;
}
