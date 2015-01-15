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
#include <net/if.h>

#include <lua.h>
#include <lauxlib.h>

#include <uv.h>

#include <inlist.h>

#include <portable_endian.h>

#include <dns_sd.h>

extern void * rt_alloc(size_t len);
extern void * rt_realloc(size_t len, void *buf);
extern void rt_free(void *buf);

typedef struct _item_t item_t;

struct _item_t {
	uv_poll_t poll;
	lua_State *L;
	DNSServiceRef ref;
};

static int
_gc(lua_State *L)
{
	item_t *item = luaL_checkudata(L, 1, "item_t");

	if(uv_is_active((uv_handle_t *)&item->poll))
		uv_poll_stop(&item->poll);
	if(item->ref)
		DNSServiceRefDeallocate(item->ref);
	item->ref = NULL;

	lua_pushlightuserdata(L, item);
	lua_pushnil(L);
	lua_rawset(L, LUA_REGISTRYINDEX);

	return 0;
}

static const luaL_Reg litem [] = {
	{"__gc", _gc},
	{NULL, NULL}
};

static void
_poll_cb(uv_poll_t *poll, int status, int flags)
{
	item_t *item = poll->data;

	int err;
	if((err = DNSServiceProcessResult(item->ref)) != kDNSServiceErr_NoError)
		fprintf(stderr, "_poll_cb: dns_sd (%i)\n", err);
}

static void
_browse_cb(
	DNSServiceRef                       ref,
	DNSServiceFlags                     flags,
	uint32_t                            interface,
	DNSServiceErrorType                 err,
	const char                          *name,
	const char                          *type,
	const char                          *domain,
	void                                *context)
{
	item_t *item = context;
	lua_State *L = item->L;

	lua_pushlightuserdata(L, item);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if(!lua_isnil(L, -1))
	{
		if(err)
		{
			lua_pushnumber(L, err);
			lua_pushnil(L);
		}
		else
		{
			lua_pushnil(L);
			lua_createtable(L, 0, 6);
			{
				lua_pushboolean(L, flags & kDNSServiceFlagsMoreComing);
				lua_setfield(L, -2, "more_coming");

				lua_pushboolean(L, flags & kDNSServiceFlagsAdd);
				lua_setfield(L, -2, "add");

				char if_name [IF_NAMESIZE];
				lua_pushstring(L, if_indextoname(interface, if_name));
				lua_setfield(L, -2, "interface");

				lua_pushstring(L, name);
				lua_setfield(L, -2, "name");

				lua_pushstring(L, type);
				lua_setfield(L, -2, "type");

				lua_pushstring(L, domain);
				lua_setfield(L, -2, "domain");
			}
		}

		if(lua_pcall(L, 2, 0, 0))
		{
			fprintf(stderr, "_browse_cb: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else
		lua_pop(L, 1);

	lua_gc(L, LUA_GCSTEP, 0);
}

static int
_browse(lua_State *L)
{
	int fd;
	int err;
	item_t *item = lua_newuserdata(L, sizeof(item_t));
	memset(item, 0, sizeof(item_t));
	item->L = L;

	luaL_getmetatable(L, "item_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, 2); // callback function
	lua_rawset(L, LUA_REGISTRYINDEX);

	lua_getfield(L, 1, "interface");
	const char *if_name = luaL_optstring(L, -1, NULL);
	uint32_t interface = if_name ? if_nametoindex(if_name) : 0;
	lua_pop(L, 1);

	lua_getfield(L, 1, "type");
	const char *type = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "domain");
	const char *domain = luaL_optstring(L, -1, NULL);
	lua_pop(L, 1);
	
	if((err = DNSServiceBrowse(&item->ref, 0, interface, type, domain, _browse_cb, item)) != kDNSServiceErr_NoError)
		fprintf(stderr, "_browse: dns_sd (%i)\n", err);
	fd = DNSServiceRefSockFD(item->ref);

	uv_loop_t *loop = uv_default_loop();
	item->poll.data = item;
	if((err = uv_poll_init(loop, &item->poll, fd)))
		fprintf(stderr, "%s\n", uv_strerror(err));
	if((err = uv_poll_start(&item->poll, UV_READABLE, _poll_cb)))
		fprintf(stderr, "%s\n", uv_strerror(err));

	return 1;
}

static void
_resolve_cb(
	DNSServiceRef                       ref,
	DNSServiceFlags                     flags,
	uint32_t                            interface,
	DNSServiceErrorType                 err,
	const char                          *fullname,
	const char                          *target,
	uint16_t                            port,
	uint16_t                            len,
	const unsigned char                 *txt,
	void                                *context)
{
	item_t *item = context;
	lua_State *L = item->L;

	lua_pushlightuserdata(L, item);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if(!lua_isnil(L, -1))
	{
		if(err)
		{
			lua_pushnumber(L, err);
			lua_pushnil(L);
		}
		else
		{
			lua_pushnil(L);
			lua_createtable(L, 0, 5);
			{
				char if_name [IF_NAMESIZE];
				lua_pushstring(L, if_indextoname(interface, if_name));
				lua_setfield(L, -2, "interface");

				lua_pushstring(L, fullname);
				lua_setfield(L, -2, "fullname");

				lua_pushstring(L, target);
				lua_setfield(L, -2, "target");

				lua_pushnumber(L, be16toh(port));
				lua_setfield(L, -2, "port");

				int n = TXTRecordGetCount(len, txt);
				lua_createtable(L, 0, n);
				for(int i=0; i<n; i++)
				{
					char key [256];
					uint8_t value_len;
					const char *value;
					TXTRecordGetItemAtIndex(len, txt, i, 256, key, &value_len, (void *)&value);

					lua_pushlstring(L, value, value_len);
					lua_setfield(L, -2, key);
				}
				lua_setfield(L, -2, "txt");
			}
		}

		if(lua_pcall(L, 2, 0, 0))
		{
			fprintf(stderr, "_resolve_cb: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else
		lua_pop(L, 1);

	if(uv_is_active((uv_handle_t *)&item->poll))
		uv_poll_stop(&item->poll);
	if(item->ref)
		DNSServiceRefDeallocate(item->ref);
	item->ref = NULL;

	lua_gc(L, LUA_GCSTEP, 0);
}

static int
_resolve(lua_State *L)
{
	int fd;
	int err;
	item_t *item = lua_newuserdata(L, sizeof(item_t));
	memset(item, 0, sizeof(item_t));
	item->L = L;

	luaL_getmetatable(L, "item_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, 2); // callback function
	lua_rawset(L, LUA_REGISTRYINDEX);

	lua_getfield(L, 1, "interface");
	const char *if_name = luaL_optstring(L, -1, NULL);
	uint32_t interface = if_name ? if_nametoindex(if_name) : 0;
	lua_pop(L, 1);

	lua_getfield(L, 1, "name");
	const char *name = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "type");
	const char *type = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "domain");
	const char *domain = luaL_checkstring(L, -1);
	lua_pop(L, 1);
	
	if((err = DNSServiceResolve(&item->ref, 0, interface, name, type, domain, _resolve_cb, item)) != kDNSServiceErr_NoError)
		fprintf(stderr, "_resolve: dns_sd (%i)\n", err);
	fd = DNSServiceRefSockFD(item->ref);

	uv_loop_t *loop = uv_default_loop();
	item->poll.data = item;
	if((err = uv_poll_init(loop, &item->poll, fd)))
		fprintf(stderr, "%s\n", uv_strerror(err));
	if((err = uv_poll_start(&item->poll, UV_READABLE, _poll_cb)))
		fprintf(stderr, "%s\n", uv_strerror(err));

	return 1;
}

static void
_query_cb(
	DNSServiceRef                       ref,
	DNSServiceFlags                     flags,
	uint32_t                            interface,
	DNSServiceErrorType                 err,
	const char                          *target,
	uint16_t                            rrtype,
	uint16_t                            rrclass,
	uint16_t                            rdlen,
	const void                          *rdata,
	uint32_t                            ttl,
	void                                *context)
{
	item_t *item = context;
	lua_State *L = item->L;

	lua_pushlightuserdata(L, item);
	lua_rawget(L, LUA_REGISTRYINDEX);
	if(!lua_isnil(L, -1))
	{
		if(err)
		{
			lua_pushnumber(L, err);
			lua_pushnil(L);
		}
		else
		{
			lua_pushnil(L);
			lua_createtable(L, 0, 3);
			{
				char if_name [IF_NAMESIZE];
				lua_pushstring(L, if_indextoname(interface, if_name));
				lua_setfield(L, -2, "interface");

				lua_pushstring(L, target);
				lua_setfield(L, -2, "target");

				if(rrtype == kDNSServiceType_A)
				{
					char buffer[INET_ADDRSTRLEN];
					if(inet_ntop(AF_INET, rdata, buffer, sizeof(buffer)))
					{
						lua_pushstring(L, buffer);
						lua_setfield(L, -2, "address");
					}

					lua_pushstring(L, "inet");
					lua_setfield(L, -2, "version");
				}
				else if(rrtype == kDNSServiceType_AAAA)
				{
					char buffer[INET6_ADDRSTRLEN];
					if(inet_ntop(AF_INET6, rdata, buffer, sizeof(buffer)))
					{
						lua_pushstring(L, buffer);
						lua_setfield(L, -2, "address");
					}

					lua_pushstring(L, "inet6");
					lua_setfield(L, -2, "version");
				}
			}
		}

		if(lua_pcall(L, 2, 0, 0))
		{
			fprintf(stderr, "_query_cb: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else
		lua_pop(L, 1);

	if(uv_is_active((uv_handle_t *)&item->poll))
		uv_poll_stop(&item->poll);
	if(item->ref)
		DNSServiceRefDeallocate(item->ref);
	item->ref = NULL;

	lua_gc(L, LUA_GCSTEP, 0);
}

static int
_query(lua_State *L)
{
	int fd;
	int err;
	item_t *item = lua_newuserdata(L, sizeof(item_t));
	memset(item, 0, sizeof(item_t));
	item->L = L;

	luaL_getmetatable(L, "item_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, 2); // callback function
	lua_rawset(L, LUA_REGISTRYINDEX);

	lua_getfield(L, 1, "interface");
	const char *if_name = luaL_optstring(L, -1, NULL);
	uint32_t interface = if_name ? if_nametoindex(if_name) : 0;
	lua_pop(L, 1);

	lua_getfield(L, 1, "target");
	const char *target = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "version");
	const char *af = luaL_optstring(L, -1, "inet");
	lua_pop(L, 1);

	int AF = kDNSServiceType_A;
	if(!strcmp(af, "inet"))
		AF = kDNSServiceType_A;
	else if(!strcmp(af, "inet6"))
		AF = kDNSServiceType_AAAA;

	if((err = DNSServiceQueryRecord(&item->ref, 0, interface, target, AF, kDNSServiceClass_IN, _query_cb, item)) != kDNSServiceErr_NoError)
		fprintf(stderr, "_query: dns_sd (%i)\n", err);
	fd = DNSServiceRefSockFD(item->ref);

	uv_loop_t *loop = uv_default_loop();
	item->poll.data = item;
	if((err = uv_poll_init(loop, &item->poll, fd)))
		fprintf(stderr, "_query: %s\n", uv_strerror(err));
	if((err = uv_poll_start(&item->poll, UV_READABLE, _poll_cb)))
		fprintf(stderr, "_query: %s\n", uv_strerror(err));

	return 1;
}

static const luaL_Reg ldns_sd [] = {
	{"browse", _browse},
	{"resolve", _resolve},
	{"query", _query},
	{NULL, NULL}
};

int
luaopen_dns_sd(lua_State *L)
{
	// disable annoying warning about using avahi's snd_sd compatibility layer
	setenv("AVAHI_COMPAT_NOWARN", "1", 1);

	luaL_newmetatable(L, "item_t");
	luaL_register(L, NULL, litem);
	lua_pop(L, 1);

	luaL_register(L, "DNS_SD", ldns_sd);		

	return 1;
}
