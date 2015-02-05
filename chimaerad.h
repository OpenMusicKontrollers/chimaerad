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

#if (defined(_WIN16) || defined(_WIN32) || defined(_WIN64)) && !defined(__WINDOWS__)
#	define ZIP_EXTERN __declspec(dllexport) // needed for static linking with mingw-w64
#endif
#include <zip.h>

#ifdef __cplusplus
extern "C" {
#endif

// include Lua
#include <lua.h>

#if defined(USE_JACK)
#	define SLICE (double)0x0.00000001p0 // smallest NTP time slice
# include <inlist.h>
#	include <jack/jack.h>
#	include <jack/ringbuffer.h>
#endif

typedef struct _rtmem_t rtmem_t;
typedef struct _app_t app_t;

struct _rtmem_t {
	void *area;
	tlsf_t tlsf;
	pool_t pool;
};

#if defined(USE_JACK)
typedef struct _slave_t slave_t;
typedef struct _job_t job_t;

struct _slave_t {
	INLIST;

	app_t *app;

	JackProcessCallback process;
	void *data;

	jack_port_t *port;
	jack_ringbuffer_t *rb;
	jack_default_audio_sample_t sample;

	Inlist *messages;
};

struct _job_t {
	int add;
	slave_t *slave;
};
#endif
	
struct _app_t {
	uv_loop_t *loop;
	lua_State *L;
	rtmem_t rtmem;
	struct zip *io;

	uv_signal_t sigint;
	uv_signal_t sigterm;
#if defined(SIGQUIT)
	uv_signal_t sigquit;
#endif

#if defined(USE_JACK)
	jack_client_t *client;
	jack_ringbuffer_t *rb;
	Inlist *slaves;

	uv_async_t asio;
	jack_ringbuffer_t *rb_msg;

	uv_timer_t syncer;
	struct timespec ntp;
	jack_time_t t0;
	jack_time_t t1;
	double T;
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

uint8_t *zip_read(app_t *app, const char *key, size_t *size);

int luaopen_json(app_t *app);
int luaopen_osc(app_t *app);
int luaopen_http(app_t *app);
int luaopen_zip(app_t *app);
int luaopen_rtmidi(app_t *app);
int luaopen_iface(app_t *app);
int luaopen_dns_sd(app_t *app);
#if defined(USE_JACK)
void rt_printf(app_t *app, const char *fmt, ...);
int luaopen_jack_midi(app_t *app);
int luaopen_jack_osc(app_t *app);
int luaopen_jack_cv(app_t *app);
#endif

#ifdef __cplusplus
}
#endif

#endif // _CHIMAERAD_H
