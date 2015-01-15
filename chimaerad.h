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

#ifndef _CHIMAERAD_H
#define _CHIMAERAD_H

#include <lua.h>

int luaopen_json(lua_State *L);
int luaopen_osc(lua_State *L);
int luaopen_http(lua_State *L);
int luaopen_zip(lua_State *L);
int luaopen_rtmidi(lua_State *L);
int luaopen_iface(lua_State *L);
int luaopen_dns_sd(lua_State *L);

#endif // _CHIMAERAD_H
