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
#include <rtmidi_c.h>

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
	EINA_INLIST;

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
	EINA_INLIST;
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
	EINA_INLIST;
	char *name;
	char ip [24];
	uint32_t ip4;
	uint32_t mask;
};

struct _chimaerad_host_t {
	Eina_Inlist *sources; // aka chimaera
	Eina_Inlist *ifaces;
	RtMidiC_Out *midi;	

	// http
	uv_tcp_t http_server;
	Eina_Inlist *http_clients;
	http_parser_settings http_settings;

	// broadcast discover
	osc_stream_t discover;

	// eet
	Eet_File *eet;
};

int chimaerad_host_init(uv_loop_t *loop, chimaerad_host_t *host, uint16_t port);
int chimaerad_host_deinit(chimaerad_host_t *host);
chimaerad_source_t * host_find_source(chimaerad_host_t *host, const char *uid);

void chimaerad_client_after_write(uv_write_t *req, int status);
int chimaerad_client_dispatch_json(chimaerad_client_t *client, cJSON *query);

void chimaerad_discover_recv_cb(osc_stream_t *stream, osc_data_t *buf, size_t len , void *data);
void chimaerad_discover_send_cb(osc_stream_t *stream, size_t len , void *data);

int chimaerad_source_init(uv_loop_t *loop, chimaerad_source_t *source);
int chimaerad_source_deinit(chimaerad_source_t *source);

#endif // _CHIMAERAD_H
