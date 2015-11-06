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

// include libuv
#include <uv.h>

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) && !defined(__WINDOWS__)
#	define __WINDOWS__
#endif

#ifdef __WINDOWS__
#	define ZIP_EXTERN __declspec(dllexport) // needed for static linking with mingw-w64
#endif
#include <zip.h>

#ifdef __cplusplus
extern "C" {
#endif

// include Lua
#include <lua.h>

typedef struct _app_t app_t;

struct _app_t {
	uv_loop_t *loop;
	lua_State *L;
	struct zip *io;

	uv_signal_t sigint;
	uv_signal_t sigterm;
#if defined(SIGQUIT)
	uv_signal_t sigquit;
#endif
};

uint8_t *zip_read(app_t *app, const char *key, size_t *size);

int luaopen_json(app_t *app);
int luaopen_osc(app_t *app);
int luaopen_http(app_t *app);
int luaopen_zip(app_t *app);
int luaopen_iface(app_t *app);
int luaopen_dns_sd(app_t *app);

#ifdef __cplusplus
}
#endif

#endif // _CHIMAERAD_H
