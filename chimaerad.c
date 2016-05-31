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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// include main header
#include <chimaerad.h>

// include Lua
#include <lualib.h>
#include <lauxlib.h>

#if defined(__WINDOWS__)
#	include <avrt.h>
#else
#	include <sys/mman.h>
#	include <sched.h>
#	include <pthread.h>
#	if defined(__APPLE__)
#		include <mach/thread_policy.h>
#		include <mach/thread_act.h>
#		include <CoreAudio/HostTime.h>
#	endif
#endif

#define AREA_SIZE 0x2000000UL // 32MB TODO increase dynamically

static void
_deinit(app_t *app)
{
	lua_close(app->L);

	uv_signal_stop(&app->sigint);
	uv_signal_stop(&app->sigterm);
#if defined(SIGQUIT)
	uv_signal_stop(&app->sigquit);
#endif
}

static void
_sig(uv_signal_t *handle, int signum)
{
	app_t *app = handle->data;

	_deinit(app);
}

static int
_zip_loader(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	const char *module = luaL_checkstring(L, 1);

	char key [256];
	sprintf(key, "%s.lua", module);

	size_t size;
	uint8_t *chunk = zip_read(app, key, &size);
	if(chunk)
	{
		luaL_loadbuffer(L, (char *)chunk, size, module);
		free(chunk);
	}
	else
		lua_pushstring(L, "module not found");

	return 1;
}

int
main(int argc, char **argv)
{
	static app_t app;
	int err;

	// use default uv_loop
	app.loop = uv_default_loop();

	app.L = luaL_newstate();
	if(!app.L)
		fprintf(stderr, "failed to create Lua state\n");

	luaL_openlibs(app.L);
	luaopen_json(&app);
	luaopen_osc(&app);
	luaopen_http(&app);
	luaopen_zip(&app);
	luaopen_iface(&app);
	luaopen_dns_sd(&app);

	app.io = zip_open(argv[1], ZIP_CHECKCONS, &err);
	if(!app.io)
		fprintf(stderr, "zip_open: %i\n", err);
	
	lua_getglobal(app.L, "package");
	if(lua_istable(app.L, -1))
	{
		lua_newtable(app.L);
		{
			lua_pushlightuserdata(app.L, &app);
			lua_pushcclosure(app.L, _zip_loader, 1);
			lua_rawseti(app.L, -2, 1);
		}
		lua_setfield(app.L, -2, "searchers");
	}
	lua_pop(app.L, 1); // package

	if(luaL_dostring(app.L, "_main = require('main')"))
	{
		fprintf(stderr, "main: %s\n", lua_tostring(app.L, -1));
		lua_pop(app.L, 1);
	}
	
	app.sigint.data = &app;
	if((err = uv_signal_init(app.loop, &app.sigint)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&app.sigint, _sig, SIGINT)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));

	app.sigterm.data = &app;
	if((err = uv_signal_init(app.loop, &app.sigterm)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&app.sigterm, _sig, SIGTERM)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));

#if defined(SIGQUIT)
	app.sigquit.data = &app;
	if((err = uv_signal_init(app.loop, &app.sigquit)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&app.sigquit, _sig, SIGQUIT)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
#endif

	uv_run(app.loop, UV_RUN_DEFAULT);

	if(app.io)
		zip_close(app.io);
	
	return 0;
}
