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

#include <chimaerad.h>

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#include <uv.h>

#include <inlist.h>

#include <http_parser.h>

typedef struct _server_t server_t;
typedef struct _client_t client_t;

struct _server_t {
	lua_State *L;
	uv_tcp_t http_server;
	Inlist *clients;
	http_parser_settings http_settings;
};

struct _client_t {
	INLIST;
	uv_tcp_t handle;
	http_parser parser;
	uv_write_t req;

	server_t *server;
};

static int
_send(lua_State *L)
{
	server_t *server = luaL_checkudata(L, 1, "server_t");

	//TODO
	return 0;
}

static void
_on_client_close(uv_handle_t *handle)
{
	client_t *client = handle->data;
	server_t *server = client->server;

	server->clients = inlist_remove(server->clients, INLIST_GET(client));
	rt_free(client);
}

static int
_gc(lua_State *L)
{
	server_t *server = luaL_checkudata(L, 1, "server_t");

	// close http clients
	Inlist *l;
	client_t *client;
	INLIST_FOREACH_SAFE(server->clients, l, client)
	{
		server->clients = inlist_remove(server->clients, INLIST_GET(client));
		uv_close((uv_handle_t *)&client->handle, _on_client_close);
	}

	// deinit http server
	uv_close((uv_handle_t *)&server->http_server, NULL);

	lua_pushlightuserdata(L, server);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	return 0;
}

static const luaL_Reg lmt [] = {
	{"__call", _send},
	{"__gc", _gc},
	{NULL, NULL}
};

void
_after_write(uv_write_t *req, int status)
{
	uv_tcp_t *handle = (uv_tcp_t *)req->handle;
	client_t *client = handle->data;

	uv_close((uv_handle_t *)handle, _on_client_close);
}

static int
_on_url(http_parser *parser, const char *at, size_t len)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	lua_pushlightuserdata(L, server);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if(!lua_isnil(L, -1))
	{
		lua_pushlstring(L, at, len);
		if(lua_pcall(L, 1, 1, 0))
		{
			fprintf(stderr, "_on_url: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}

		size_t size;
		const char *chunk= luaL_checklstring(L, -1, &size);
		lua_pop(L, 1);

		uv_buf_t msg [1];

		if(chunk)
		{
			msg[0].base = (char *)chunk;
			msg[0].len = size;

			uv_write(&client->req, (uv_stream_t *)&client->handle, msg, 1, _after_write);
		}
	}
	else
		lua_pop(L, 1);
		
	lua_gc(L, LUA_GCSTEP, 0);

	return 0;
}

static void
_on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = rt_alloc(suggested_size); //TODO
	buf->len = suggested_size;
}

static void
_on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	uv_tcp_t *handle = (uv_tcp_t *)stream;
	client_t *client = handle->data;
	server_t *server = client->server;

	if(nread >= 0)
	{
		size_t parsed = http_parser_execute(&client->parser, &server->http_settings, buf->base, nread);

		if(parsed < nread)
		{
			fprintf(stderr, "_on_read: http parse error\n");
			uv_close((uv_handle_t *)handle, _on_client_close);
		}
	}
	else
	{
		if(nread != UV_EOF)
			fprintf(stderr, "_on_read: %s\n", uv_strerror(nread));
		uv_close((uv_handle_t *)handle, _on_client_close);
	}

	rt_free(buf->base);
}

static void
_on_connected(uv_stream_t *handle, int status)
{
	int err;

	server_t *server = handle->data;
	//TODO check status

	client_t *client = rt_alloc(sizeof(client_t));
	if(!client)
		return;

	client->server = server;
	server->clients = inlist_append(server->clients, INLIST_GET(client));

	if((err = uv_tcp_init(handle->loop, &client->handle)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_strerror(err));
		return;
	}

	if((err = uv_accept(handle, (uv_stream_t *)&client->handle)))
	{
		fprintf(stderr, "uv_accept: %s\n", uv_strerror(err));
		return;
	}

	client->handle.data = client;
	client->parser.data = client;

	http_parser_init(&client->parser, HTTP_REQUEST);

	if((uv_read_start((uv_stream_t *)&client->handle, _on_alloc, _on_read)))
	{
		fprintf(stderr, "uv_read_start: %s\n", uv_strerror(err));
		return;
	}
}

static int
_new(lua_State *L)
{
	//uv_loop_t *loop = lua_touserdata(L, lua_upvalueindex(1)); FIXME
	uv_loop_t *loop = uv_default_loop();
	uint16_t port = luaL_checkint(L, 1);
	int has_callback = lua_gettop(L) > 1; //TODO check whether this is callable

	server_t *server = lua_newuserdata(L, sizeof(server_t));
	memset(server, 0, sizeof(server_t));
	server->L = L;
	
	server->http_settings.on_url = _on_url;
	server->http_server.data = server;

	int err;
	if((err = uv_tcp_init(loop, &server->http_server)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_strerror(err));
		return -1;
	}

	struct sockaddr_in addr_ip4;
	struct sockaddr *addr = (struct sockaddr *)&addr_ip4;

	if((err = uv_ip4_addr("0.0.0.0", port, &addr_ip4)))
	{
		fprintf(stderr, "uv_ip4_addr: %s\n", uv_strerror(err));
		return -1;
	}

	if((err = uv_tcp_bind(&server->http_server, addr, 0)))
	{
		fprintf(stderr, "bind: %s\n", uv_strerror(err));
		return -1;
	}

	if((err = uv_listen((uv_stream_t *)&server->http_server, 128, _on_connected)))
	{
		fprintf(stderr, "listen %s\n", uv_strerror(err));
		return -1;
	}

	if(0) //FIXME
	{
		lua_pop(L, 1);
		lua_pushnil(L);
	}
	else
	{
		luaL_getmetatable(L, "server_t");
		lua_setmetatable(L, -2);

		if(has_callback)
		{
			lua_pushlightuserdata(L, server);
			lua_pushvalue(L, 2); // push callback
			lua_rawset(L, LUA_REGISTRYINDEX);
		}
	}

	return 1;
}

static const luaL_Reg lhttp [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_http(lua_State *L)
{
	luaL_newmetatable(L, "server_t");
	luaL_register(L, NULL, lmt);
	lua_pop(L, 1);

	luaL_register(L, "HTTP", lhttp);		

	return 1;
}
