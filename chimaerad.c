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

#include <chimaerad.h>

#if !defined(__WINDOWS__)
#	include <sys/mman.h>
#endif

#define AREA_SIZE 0x1000000UL // 16MB TODO increase dynamically

typedef struct _rtmem_t rtmem_t;

struct _rtmem_t {
	void *area;
	tlsf_t tlsf;
	pool_t pool;
};
	
static rtmem_t rtmem;
static uv_signal_t sigint;
static uv_signal_t sigterm;
#if defined(SIGQUIT)
static uv_signal_t sigquit;
#endif

void *
rt_alloc(size_t len)
{
	void *data = NULL;
	
	if(!(data = tlsf_malloc(rtmem.tlsf, len)))
		; //FIXME

	return data;
}

void *
rt_realloc(size_t len, void *buf)
{
	void *data = NULL;
	
	if(!(data =tlsf_realloc(rtmem.tlsf, buf, len)))
		; //FIXME

	return data;
}

void
rt_free(void *buf)
{
	tlsf_free(rtmem.tlsf, buf);
}

static void *
_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	(void)ud;
	(void)osize;

	if(nsize == 0) {
		if(ptr)
			tlsf_free(rtmem.tlsf, ptr);
		return NULL;
	}
	else {
		if(ptr)
			return tlsf_realloc(rtmem.tlsf, ptr, nsize);
		else
			return tlsf_malloc(rtmem.tlsf, nsize);
	}
}

static void
_sig(uv_signal_t *handle, int signum)
{
	lua_State *L = handle->data;

	lua_close(L);

	uv_signal_stop(&sigint);
	uv_signal_stop(&sigterm);
#if defined(SIGQUIT)
	uv_signal_stop(&sigquit);
#endif
}

int
main(int argc, char **argv)
{
#if defined(__WINDOWS__)
	rtmem.area = malloc(AREA_SIZE);
#elif defined(__linux__) || defined(__CYGWIN__)
	rtmem.area = mmap(NULL, AREA_SIZE, PROT_READ|PROT_WRITE, MAP_32BIT|MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);
#elif defined(__APPLE__)
	rtmem.area = mmap(NULL, AREA_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANON, -1, 0);
#endif
	rtmem.tlsf = tlsf_create_with_pool(rtmem.area, AREA_SIZE);
	rtmem.pool = tlsf_get_pool(rtmem.tlsf);

	uv_loop_t *loop = uv_default_loop();
	lua_State *L = lua_newstate(_lua_alloc, NULL);

	luaL_openlibs(L);
	luaopen_json(L);
	luaopen_osc(L);
	luaopen_http(L);
	luaopen_zip(L);
	luaopen_rtmidi(L);
	luaopen_iface(L);
	lua_gc(L, LUA_GCSTOP, 0);

	if(luaL_dofile(L, argv[1]))
		fprintf(stderr, "main: %s\n", lua_tostring(L, -1));
	
	int err;
	sigint.data = L;
	if((err = uv_signal_init(loop, &sigint)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&sigint, _sig, SIGINT)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));

	sigterm.data = L;
	if((err = uv_signal_init(loop, &sigterm)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&sigterm, _sig, SIGTERM)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));

#if defined(SIGQUIT)
	sigquit.data = L;
	if((err = uv_signal_init(loop, &sigquit)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
	if((err = uv_signal_start(&sigquit, _sig, SIGQUIT)))
		fprintf(stderr, "uv error: %s\n", uv_err_name(err));
#endif

	uv_run(loop, UV_RUN_DEFAULT);

	tlsf_remove_pool(rtmem.tlsf, rtmem.pool);
	tlsf_destroy(rtmem.tlsf);
#if defined(__WINDOWS__)
	free(rtmem.area);
#else
	munmap(rtmem.area, AREA_SIZE);
#endif

	return 0;
}
