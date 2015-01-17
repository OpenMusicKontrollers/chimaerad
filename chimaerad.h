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

// include TLSF
#include <tlsf.h>

#ifdef __cplusplus
extern "C" {
#endif

// include Lua
#include <lua.h>

typedef struct _rtmem_t rtmem_t;
typedef struct _app_t app_t;

struct _rtmem_t {
	void *area;
	tlsf_t tlsf;
	pool_t pool;
};
	
struct _app_t {
	uv_loop_t *loop;
	lua_State *L;
	rtmem_t rtmem;

	uv_signal_t sigint;
	uv_signal_t sigterm;
#if defined(SIGQUIT)
	uv_signal_t sigquit;
#endif
};

static inline void *
rt_alloc(app_t *app, size_t len)
{
	return tlsf_malloc(app->rtmem.tlsf, len);
}

static inline void *
rt_realloc(app_t *app, size_t len, void *buf)
{
	return tlsf_realloc(app->rtmem.tlsf, buf, len);
}

static inline void
rt_free(app_t *app, void *buf)
{
	tlsf_free(app->rtmem.tlsf, buf);
}

int luaopen_json(app_t *app);
int luaopen_osc(app_t *app);
int luaopen_http(app_t *app);
int luaopen_zip(app_t *app);
int luaopen_rtmidi(app_t *app);
int luaopen_iface(app_t *app);
int luaopen_dns_sd(app_t *app);

#ifdef __cplusplus
}
#endif

#endif // _CHIMAERAD_H
