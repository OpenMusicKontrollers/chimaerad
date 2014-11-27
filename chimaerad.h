#ifndef _CHIMAERAD_H
#define _CHIMAERAD_H

// include libuv
#include <uv.h>

// include Lua
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

// include Efl
#include <Eina.h>
#include <Eet.h>

// include varia
#include <osc_stream.h>
#include <http_parser.h>
#include <tlsf.h>
#include <dns_sd.h>
#include <cJSON.h>

typedef struct _chimaerad_source_t chimaerad_source_t;
typedef struct _chimaerad_sink_t chimaerad_sink_t;
typedef struct _chimaerad_host_t chimaerad_host_t;
typedef struct _chimaerad_client_t chimaerad_client_t;
typedef struct _chimaerad_method_t chimaerad_method_t;

typedef int (*chimaerad_method_cb_t)(chimaerad_client_t *client, cJSON *osc);

typedef enum _chimaerad_source_type_t chimaerad_source_type_t;
typedef enum _chimaerad_sink_type_t chimaerad_sink_type_t;

enum _chimaerad_source_type_t {
	CHIMAERA_SOURCE_TYPE_UDP,
	CHIMAERA_SOURCE_TYPE_TCP
};

enum _chimaerad_sink_type_t {
	CHIMAERA_SINK_TYPE_MIDI,
	CHIMAERA_SINK_TYPE_OSC,
	//CHIMAERA_SINK_TYPE_JACK_MIDI,
	//CHIMAERA_SINK_TYPE_JACK_OSC,
	//CHIMAERA_SINK_TYPE_JACK_CV
};

struct _chimaerad_source_t {
	EINA_INLIST;
	chimaerad_source_type_t type;
	lua_State *L;
	uv_thread_t *thread;
	uv_loop_t *loop;
	osc_stream_t config;
	osc_stream_t data;
	//TODO
};

struct _chimaerad_sink_t {
	EINA_INLIST;
	chimaerad_sink_type_t type;
	//TODO
};

struct _chimaerad_client_t {
	EINA_INLIST;
	uv_tcp_t handle;
	http_parser parser;
	uv_write_t req;
	chimaerad_host_t *host;
	char *chunk;
};

struct _chimaerad_method_t {
	const char *path; 
	const char *fmt;
	chimaerad_method_cb_t cb;
};

struct _chimaerad_host_t {
	Eina_Inlist *sources;
	Eina_Inlist *sinks;

	lua_State *L;

	// http
	uv_tcp_t http_server;
	Eina_Inlist *http_clients;
	http_parser_settings http_settings;

	// ping-sd
	osc_stream_t ping;

	// eet
	Eet_File *eet;
};

int chimaerad_host_init(uv_loop_t *loop, chimaerad_host_t *host, uint16_t port);
int chimaerad_host_deinit(chimaerad_host_t *host);

int chimaerad_dummy_init(uv_loop_t *loop, uint16_t port);
int chimaerad_dummy_deinit();

#endif // _CHIMAERAD_H
