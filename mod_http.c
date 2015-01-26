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
	app_t *app;
};

struct _client_t {
	INLIST;
	uv_tcp_t handle;
	http_parser parser;
	uv_write_t req;

	server_t *server;
};

static inline void
_client_remove(client_t *client)
{
	server_t *server = client->server;
	lua_State *L = server->L;

	server->clients = inlist_remove(server->clients, INLIST_GET(client));

	lua_pushlightuserdata(L, client);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
}

static void
_on_client_close(uv_handle_t *handle)
{
	client_t *client = handle->data;

	_client_remove(client);
}

void
_after_write(uv_write_t *req, int status)
{
	uv_tcp_t *handle = (uv_tcp_t *)req->handle;
	client_t *client = handle->data;
	server_t *server = client->server;
	int err;

	if(req->data)
		rt_free(server->app, req->data);

	if(uv_is_active((uv_handle_t *)handle))
	{
		if((err = uv_read_stop((uv_stream_t *)handle)))
			fprintf(stderr, "uv_read_stop: %s\n", uv_strerror(err));
		uv_close((uv_handle_t *)handle, _on_client_close);
	}
}

static int
_client_send(lua_State *L)
{
	client_t *client = luaL_checkudata(L, 1, "client_t");
	server_t *server = client->server;

	size_t size;
	const char *chunk= luaL_checklstring(L, -1, &size);
	lua_pop(L, 1);

	if(chunk)
	{
		client->req.data = rt_alloc(server->app, size);
		if(!client->req.data)
			return 0;
		memcpy(client->req.data, chunk, size);

		uv_buf_t msg = {
			.base = client->req.data,
			.len = size
		};

		uv_write(&client->req, (uv_stream_t *)&client->handle, &msg, 1, _after_write);
	}

	return 0;
}

static const luaL_Reg lclient [] = {
	{"__call", _client_send},
	{NULL, NULL}
};

static int
_server_gc(lua_State *L)
{
	server_t *server = luaL_checkudata(L, 1, "server_t");
	int err;

	// close http clients
	Inlist *l;
	client_t *client;
	INLIST_FOREACH_SAFE(server->clients, l, client)
	{
		if(uv_is_active((uv_handle_t *)&client->handle))
		{
			if((err = uv_read_stop((uv_stream_t *)&client->handle)))
				fprintf(stderr, "uv_read_stop: %s\n", uv_strerror(err));
			uv_close((uv_handle_t *)&client->handle, NULL);
		}

		_client_remove(client);
	}

	// deinit http server
	if(uv_is_active((uv_handle_t *)&server->http_server))
		uv_close((uv_handle_t *)&server->http_server, NULL);

	lua_pushlightuserdata(L, server);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	return 0;
}

static const luaL_Reg lserver [] = {
	{"__gc", _server_gc},
	{"close", _server_gc},
	{NULL, NULL}
};

static int
_on_message_begin(http_parser *parser)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	lua_pushlightuserdata(L, parser);
	lua_newtable(L);

	lua_newtable(L);
	lua_setfield(L, -2, "header");

	lua_rawset(L, LUA_REGISTRYINDEX);

	return 0;
}

static int
_on_headers_complete(http_parser *parser)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	// do nothing

	return 0;
}

static int
_on_message_complete(http_parser *parser)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	lua_pushlightuserdata(L, server);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if(!lua_isnil(L, -1))
	{
		lua_pushlightuserdata(L, client);
		lua_rawget(L, LUA_REGISTRYINDEX);

		lua_pushlightuserdata(L, parser);
		lua_rawget(L, LUA_REGISTRYINDEX);

		if(lua_pcall(L, 2, 0, 0))
		{
			fprintf(stderr, "_on_message_complete: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else
		lua_pop(L, 1);

	// free temporary table
	lua_pushlightuserdata(L, parser);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);
		
	lua_gc(L, LUA_GCCOLLECT, 0);

	return 0;
}

static int
_on_header_field(http_parser *parser, const char *at, size_t len)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	lua_pushlightuserdata(L, parser);
	lua_rawget(L, LUA_REGISTRYINDEX);

	lua_getfield(L, -1, "header");

	lua_pushlstring(L, at, len);
	lua_pushboolean(L, 0);
	lua_rawset(L, -3);

	lua_pop(L, 2);

	return 0;
}

static int
_on_header_value(http_parser *parser, const char *at, size_t len)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	lua_pushlightuserdata(L, parser);
	lua_rawget(L, LUA_REGISTRYINDEX);

	lua_getfield(L, -1, "header");
	
	lua_pushnil(L);
	while(lua_next(L, -2))
	{
		if(lua_type(L, -1) == LUA_TBOOLEAN)
		{
			lua_pop(L, 1); // pop false
			lua_pushlstring(L, at, len);
			lua_rawset(L, -3);
			break;
		}

		lua_pop(L, 1);
	}

	lua_pop(L, 2);

	return 0;
}

static int
_on_url(http_parser *parser, const char *at, size_t len)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	lua_pushlightuserdata(L, parser);
	lua_rawget(L, LUA_REGISTRYINDEX);

	lua_pushlstring(L, at, len);
	lua_setfield(L, -2, "url");

	lua_pop(L, 1);

	return 0;
}

static int
_on_body(http_parser *parser, const char *at, size_t len)
{
	client_t *client = parser->data;
	server_t *server = client->server;
	lua_State *L = server->L;

	lua_pushlightuserdata(L, parser);
	lua_rawget(L, LUA_REGISTRYINDEX);

	lua_pushlstring(L, at, len);
	lua_setfield(L, -2, "body");

	lua_pop(L, 1);

	return 0;
}

static void
_on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	client_t *client = handle->data;

	buf->base = rt_alloc(client->server->app, suggested_size);
	buf->len = buf->base ? suggested_size : 0;
}

static void
_on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	uv_tcp_t *handle = (uv_tcp_t *)stream;
	client_t *client = handle->data;
	server_t *server = client->server;
	int err;

	if(nread >= 0)
	{
		size_t parsed = http_parser_execute(&client->parser, &server->http_settings, buf->base, nread);

		if(parsed < nread)
		{
			fprintf(stderr, "_on_read: http parse error\n");
			if(uv_is_active((uv_handle_t *)handle))
			{
				if((err = uv_read_stop((uv_stream_t *)handle)))
					fprintf(stderr, "uv_read_stop: %s\n", uv_strerror(err));
				uv_close((uv_handle_t *)handle, _on_client_close);
			}
		}
	}
	else
	{
		if(nread != UV_EOF)
			fprintf(stderr, "_on_read: %s\n", uv_strerror(nread));
		if(uv_is_active((uv_handle_t *)handle))
		{
			if((err = uv_read_stop((uv_stream_t *)handle)))
				fprintf(stderr, "uv_read_stop: %s\n", uv_strerror(err));
			uv_close((uv_handle_t *)handle, _on_client_close);
		}
	}

	if(buf->base)
		rt_free(client->server->app, buf->base);
}

static void
_on_connected(uv_stream_t *handle, int status)
{
	int err;

	server_t *server = handle->data;
	lua_State *L = server->L;
	//TODO check status

	client_t *client = lua_newuserdata(L, sizeof(client_t));
	if(!client)
		return;
	memset(client, 0, sizeof(client_t));

	luaL_getmetatable(L, "client_t");
	lua_setmetatable(L, -2);
	
	lua_pushlightuserdata(L, client);
	lua_insert(L, -2);
	lua_rawset(L, LUA_REGISTRYINDEX);

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

	if((err = uv_read_start((uv_stream_t *)&client->handle, _on_alloc, _on_read)))
	{
		fprintf(stderr, "uv_read_start: %s\n", uv_strerror(err));
		return;
	}
}

static int
_new(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	uint16_t port = luaL_checkint(L, 1);

	server_t *server = lua_newuserdata(L, sizeof(server_t));
	if(!server)
		goto fail;
	memset(server, 0, sizeof(server_t));
	server->L = L;

	server->app = app;
	server->http_settings.on_message_begin = _on_message_begin;
	server->http_settings.on_message_complete= _on_message_complete;
	server->http_settings.on_headers_complete= _on_headers_complete;

	server->http_settings.on_url = _on_url;
	server->http_settings.on_header_field = _on_header_field;
	server->http_settings.on_header_value = _on_header_value;
	server->http_settings.on_body = _on_body;
	server->http_server.data = server;

	int err;
	if((err = uv_tcp_init(app->loop, &server->http_server)))
	{
		fprintf(stderr, "uv_tcp_init: %s\n", uv_strerror(err));
		goto fail;
	}

	struct sockaddr_in addr_ip4;
	struct sockaddr *addr = (struct sockaddr *)&addr_ip4;

	if((err = uv_ip4_addr("0.0.0.0", port, &addr_ip4)))
	{
		fprintf(stderr, "uv_ip4_addr: %s\n", uv_strerror(err));
		goto fail;
	}

	if((err = uv_tcp_bind(&server->http_server, addr, 0)))
	{
		fprintf(stderr, "bind: %s\n", uv_strerror(err));
		goto fail;
	}

	if((err = uv_listen((uv_stream_t *)&server->http_server, 128, _on_connected)))
	{
		fprintf(stderr, "listen %s\n", uv_strerror(err));
		goto fail;
	}

	luaL_getmetatable(L, "server_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, server);
	lua_pushvalue(L, 2); // push callback
	lua_rawset(L, LUA_REGISTRYINDEX);

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg lhttp [] = {
	{"new", _new},
	{NULL, NULL}
};

int
luaopen_http(app_t *app)
{
	lua_State *L = app->L;

	luaL_newmetatable(L, "server_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, lserver, 1);
	lua_pop(L, 1);
	
	luaL_newmetatable(L, "client_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, lclient, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "HTTP", lhttp, 1);

	return 1;
}
