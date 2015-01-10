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

// include Lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// include varia
#include <osc_stream.h>
#include <http_parser.h>
#include <tlsf.h>
#include <cJSON.h>
#include <rtmidi_c.h>

#ifdef __WINDOWS__
#	define ZIP_EXTERN __declspec(dllexport) // needed for static linking with mingw-w64
#endif
#include <zip.h>

typedef struct _chimaerad_source_t chimaerad_source_t;
typedef struct _chimaerad_iface_t chimaerad_iface_t;
typedef struct _chimaerad_host_t chimaerad_host_t;
typedef struct _chimaerad_client_t chimaerad_client_t;
typedef struct _chimaerad_method_t chimaerad_method_t;

typedef int (*chimaerad_method_cb_t)(chimaerad_client_t *client, cJSON *osc);

typedef enum _chimaerad_source_mode_t chimaerad_source_mode_t;
typedef enum _chimaerad_source_lease_t chimaerad_source_lease_t;

enum _chimaerad_source_mode_t {
	CHIMAERAD_SOURCE_MODE_UDP	= 0,
	CHIMAERAD_SOURCE_MODE_TCP	= 1
};

enum _chimaerad_source_lease_t {
	CHIMAERAD_SOURCE_LEASE_DHCP	= 0,
	CHIMAERAD_SOURCE_LEASE_IPV4LL	= 1,
	CHIMAERAD_SOURCE_LEASE_STATIC	= 2
};

struct _chimaerad_source_t {
	INLIST;

	chimaerad_host_t *host;

	// as returned from discover
	char *uid;
	char *name;
	char *ip;
	int claimed;
	chimaerad_source_mode_t mode;
	chimaerad_source_lease_t lease;
	int rate;

	uint32_t ip4;
	uint32_t mask;
	int reachable;
	chimaerad_iface_t *iface;

	uv_loop_t loop;
	uv_async_t quit;
	uv_thread_t thread;

	osc_stream_t config;
	osc_stream_t data;

	lua_State *L;
	void *area;
	tlsf_t tlsf;
	pool_t pool;
	//TODO
};

struct _chimaerad_client_t {
	INLIST;
	uv_tcp_t handle;
	http_parser parser;
	uv_write_t req;
	chimaerad_host_t *host;
	char *chunk;
};

struct _chimaerad_method_t {
	const char *path; 
	chimaerad_method_cb_t cb;
};

struct _chimaerad_iface_t {
	INLIST;
	char *name;
	char ip [24];
	uint32_t ip4;
	uint32_t mask;
};

struct _chimaerad_host_t {
	Inlist *sources; // aka chimaera
	Inlist *ifaces;
	RtMidiC_Out *midi;	

	// http
	uv_tcp_t http_server;
	Inlist *http_clients;
	http_parser_settings http_settings;

	// broadcast discover
	osc_stream_t discover;

	// libzip
	struct zip *z;
};

int luaopen_json(lua_State *L);
int luaopen_osc(lua_State *L);
int luaopen_http(lua_State *L);
int luaopen_zip(lua_State *L);
int luaopen_rtmidi(lua_State *L);
int luaopen_iface(lua_State *L);

int chimaerad_host_init(uv_loop_t *loop, chimaerad_host_t *host, uint16_t port);
int chimaerad_host_deinit(chimaerad_host_t *host);
chimaerad_source_t * host_find_source(chimaerad_host_t *host, const char *uid);
char *host_zip_file(chimaerad_host_t *host, const char *path, size_t *size);

void chimaerad_client_after_write(uv_write_t *req, int status);
int chimaerad_client_dispatch_json(chimaerad_client_t *client, cJSON *query);

void chimaerad_discover_recv_cb(osc_stream_t *stream, osc_data_t *buf, size_t len , void *data);
void chimaerad_discover_send_cb(osc_stream_t *stream, size_t len , void *data);

int chimaerad_source_init(uv_loop_t *loop, chimaerad_source_t *source);
int chimaerad_source_deinit(chimaerad_source_t *source);

#endif // _CHIMAERAD_H
