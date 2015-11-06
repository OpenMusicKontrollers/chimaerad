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
#include <stdint.h>

#include <chimaerad.h>

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#include <uv.h>

#include <inlist.h>

#if defined(__WINDOWS__)
#	include <netioapi.h>
#	ifndef IF_NAMESIZE
#		define IF_NAMESIZE 64 // FIXME if_nametoindex, if_indextoname only supported >= Vista
#	endif
#else
#	include <net/if.h>
#endif

#include <dns_sd.h>

typedef struct _item_t item_t;

struct _item_t {
	uv_poll_t poll;
	lua_State *L;
	DNSServiceRef ref;
	const char *fullname;
};

static int
_gc(lua_State *L)
{
	item_t *item = luaL_checkudata(L, 1, "item_t");
	if(!item)
		return 0;

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
	{"close", _gc},
	{NULL, NULL}
};

static void
_poll_cb(uv_poll_t *poll, int status, int flags)
{
	item_t *item = poll->data;
	if(!item)
		return;

	int err;
	if((err = DNSServiceProcessResult(item->ref)) != kDNSServiceErr_NoError)
		fprintf(stderr, "_poll_cb: dns_sd (%i)\n", err);
}

static inline size_t
_strlen_dot(const char *str)
{
	size_t len = strlen(str);

	return str[len-1] == '.' ? len-1 : len;
}

static void DNSSD_API
_browse_cb(
	DNSServiceRef				ref,
	DNSServiceFlags			flags,
	uint32_t						iface,
	DNSServiceErrorType	err,
	const char					*name,
	const char					*type,
	const char					*domain,
	void								*context)
{
	item_t *item = context;
	if(!item)
		return;

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

#if defined(IF_NAMESIZE)
				char if_name [IF_NAMESIZE];
				lua_pushstring(L, if_indextoname(iface, if_name));
				lua_setfield(L, -2, "interface");
#endif

				lua_pushlstring(L, name, _strlen_dot(name));
				lua_setfield(L, -2, "name");

				lua_pushlstring(L, type, _strlen_dot(type));
				lua_setfield(L, -2, "type");

				lua_pushlstring(L, domain, _strlen_dot(domain));
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

	lua_gc(L, LUA_GCCOLLECT, 0);
}

static int
_browse(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	int fd;
	int err;
	DNSServiceFlags flags = 0;
	item_t *item = lua_newuserdata(L, sizeof(item_t));
	if(!item)
		goto fail;
	memset(item, 0, sizeof(item_t));
	item->L = L;

	luaL_getmetatable(L, "item_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, 2); // callback function
	lua_rawset(L, LUA_REGISTRYINDEX);

#if defined(IF_NAMESIZE)
	lua_getfield(L, 1, "interface");
	const char *if_name = luaL_optstring(L, -1, NULL);
	uint32_t iface = if_name ? if_nametoindex(if_name) : 0;
	lua_pop(L, 1);
#else
	uint32_t iface = 0;
#endif

	lua_getfield(L, 1, "type");
	const char *type = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "domain");
	const char *domain = luaL_optstring(L, -1, NULL);
	lua_pop(L, 1);
	
	if((err = DNSServiceBrowse(&item->ref, flags, iface, type, domain, _browse_cb, item)) != kDNSServiceErr_NoError)
	{
		fprintf(stderr, "_browse: dns_sd (%i)\n", err);
		goto fail;
	}
	if((fd = DNSServiceRefSockFD(item->ref)) < 0)
		goto fail;

	item->poll.data = item;
	if((err = uv_poll_init_socket(app->loop, &item->poll, fd)))
	{
		fprintf(stderr, "%s\n", uv_strerror(err));
		goto fail;
	}
	if((err = uv_poll_start(&item->poll, UV_READABLE, _poll_cb)))
	{
		fprintf(stderr, "%s\n", uv_strerror(err));
		goto fail;
	}

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static void DNSSD_API
_resolve_cb(
	DNSServiceRef				ref,
	DNSServiceFlags			flags,
	uint32_t						iface,
	DNSServiceErrorType	err,
	const char					*fullname,
	const char					*target,
	uint16_t						port,
	uint16_t						len,
	const unsigned char	*txt,
	void								*context)
{
	item_t *item = context;
	if(!item)
		return;

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
#if defined(IF_NAMESIZE)
				char if_name [IF_NAMESIZE];
				lua_pushstring(L, if_indextoname(iface, if_name));
				lua_setfield(L, -2, "interface");
#endif

				lua_pushlstring(L, fullname, _strlen_dot(fullname));
				lua_setfield(L, -2, "fullname");

				lua_pushlstring(L, target, _strlen_dot(target));
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

	lua_gc(L, LUA_GCCOLLECT, 0);
}

static int
_resolve(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	int fd;
	int err;
	DNSServiceFlags flags = 0;
	item_t *item = lua_newuserdata(L, sizeof(item_t));
	if(!item)
		goto fail;
	memset(item, 0, sizeof(item_t));
	item->L = L;

	luaL_getmetatable(L, "item_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, 2); // callback function
	lua_rawset(L, LUA_REGISTRYINDEX);

#if defined(IF_NAMESIZE)
	lua_getfield(L, 1, "interface");
	const char *if_name = luaL_optstring(L, -1, NULL);
	uint32_t iface = if_name ? if_nametoindex(if_name) : 0;
	lua_pop(L, 1);
#else
	uint32_t iface = 0;
#endif

	lua_getfield(L, 1, "name");
	const char *name = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "type");
	const char *type = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "domain");
	const char *domain = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	if((err = DNSServiceResolve(&item->ref, flags, iface, name, type, domain, _resolve_cb, item)) != kDNSServiceErr_NoError)
	{
		fprintf(stderr, "_resolve: dns_sd (%i)\n", err);
		goto fail;
	}
	if((fd = DNSServiceRefSockFD(item->ref)) < 0)
		goto fail;

	item->poll.data = item;
	if((err = uv_poll_init_socket(app->loop, &item->poll, fd)))
	{
		fprintf(stderr, "%s\n", uv_strerror(err));
		goto fail;
	}
	if((err = uv_poll_start(&item->poll, UV_READABLE, _poll_cb)))
	{
		fprintf(stderr, "%s\n", uv_strerror(err));
		goto fail;
	}

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static void DNSSD_API
_query_ip_cb(
	DNSServiceRef				ref,
	DNSServiceFlags			flags,
	uint32_t						iface,
	DNSServiceErrorType	err,
	const char					*target,
	uint16_t						rrtype,
	uint16_t						rrclass,
	uint16_t						rdlen,
	const void					*rdata,
	uint32_t						ttl,
	void								*context)
{
	item_t *item = context;
	if(!item)
		return;

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
			lua_createtable(L, 0, 7);
			{
				lua_pushboolean(L, flags & kDNSServiceFlagsMoreComing);
				lua_setfield(L, -2, "more_coming");

				lua_pushboolean(L, flags & kDNSServiceFlagsAdd);
				lua_setfield(L, -2, "add");

#if defined(IF_NAMESIZE)
				char if_name [IF_NAMESIZE];
				lua_pushstring(L, if_indextoname(iface, if_name));
				lua_setfield(L, -2, "interface");
#endif

				lua_pushstring(L, item->fullname);
				lua_setfield(L, -2, "fullname");

				lua_pushlstring(L, target, _strlen_dot(target));
				lua_setfield(L, -2, "target");

				if(rrtype == kDNSServiceType_A)
				{
					char buffer[INET_ADDRSTRLEN];
					if(!uv_inet_ntop(AF_INET, rdata, buffer, sizeof(buffer)))
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
					if(!uv_inet_ntop(AF_INET6, rdata, buffer, sizeof(buffer)))
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
			fprintf(stderr, "_query_ip_cb: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else
		lua_pop(L, 1);

	// Query request are kept running to listen for IP changes

	lua_gc(L, LUA_GCCOLLECT, 0);
}

static int
_query_ip(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	int fd;
	int err;
	DNSServiceFlags flags = 0;
	item_t *item = lua_newuserdata(L, sizeof(item_t));
	if(!item)
		goto fail;
	memset(item, 0, sizeof(item_t));
	item->L = L;

	luaL_getmetatable(L, "item_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, 2); // callback function
	lua_rawset(L, LUA_REGISTRYINDEX);

#if defined(IF_NAMESIZE)
	lua_getfield(L, 1, "interface");
	const char *if_name = luaL_optstring(L, -1, NULL);
	uint32_t iface = if_name ? if_nametoindex(if_name) : 0;
	lua_pop(L, 1);
#else
	uint32_t iface = 0;
#endif

	lua_getfield(L, 1, "target");
	const char *target = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "fullname");
	item->fullname = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "version");
	const char *af = luaL_optstring(L, -1, "inet");
	lua_pop(L, 1);

	int AF = kDNSServiceType_A;
	if(!strcmp(af, "inet"))
		AF = kDNSServiceType_A;
	else if(!strcmp(af, "inet6"))
		AF = kDNSServiceType_AAAA;

	if((err = DNSServiceQueryRecord(&item->ref, flags, iface, target, AF, kDNSServiceClass_IN, _query_ip_cb, item)) != kDNSServiceErr_NoError)
	{
		fprintf(stderr, "_query_ip: dns_sd (%i)\n", err);
		goto fail;
	}
	if((fd = DNSServiceRefSockFD(item->ref)) < 0)
		goto fail;

	item->poll.data = item;
	if((err = uv_poll_init_socket(app->loop, &item->poll, fd)))
	{
		fprintf(stderr, "_query_ip: %s\n", uv_strerror(err));
		goto fail;
	}
	if((err = uv_poll_start(&item->poll, UV_READABLE, _poll_cb)))
	{
		fprintf(stderr, "_query_ip: %s\n", uv_strerror(err));
		goto fail;
	}

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static void DNSSD_API
_query_txt_cb(
	DNSServiceRef				ref,
	DNSServiceFlags			flags,
	uint32_t						iface,
	DNSServiceErrorType	err,
	const char					*target,
	uint16_t						rrtype,
	uint16_t						rrclass,
	uint16_t						rdlen,
	const void					*rdata,
	uint32_t						ttl,
	void								*context)
{
	item_t *item = context;
	if(!item)
		return;

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

#if defined(IF_NAMESIZE)
				char if_name [IF_NAMESIZE];
				lua_pushstring(L, if_indextoname(iface, if_name));
				lua_setfield(L, -2, "interface");
#endif

				lua_pushstring(L, item->fullname);
				lua_setfield(L, -2, "fullname");

				lua_pushlstring(L, target, _strlen_dot(target));
				lua_setfield(L, -2, "target");

				int n = TXTRecordGetCount(rdlen, rdata);
				lua_createtable(L, 0, n);
				for(int i=0; i<n; i++)
				{
					char key [256];
					uint8_t value_len;
					const char *value;
					TXTRecordGetItemAtIndex(rdlen, rdata, i, 256, key, &value_len, (void *)&value);

					lua_pushlstring(L, value, value_len);
					lua_setfield(L, -2, key);
				}
				lua_setfield(L, -2, "txt");
			}
		}

		if(lua_pcall(L, 2, 0, 0))
		{
			fprintf(stderr, "_query_txt_cb: %s\n", lua_tostring(L, -1));
			lua_pop(L, 1);
		}
	}
	else
		lua_pop(L, 1);

	// Query request are kept running to listen for TXT changes

	lua_gc(L, LUA_GCCOLLECT, 0);
}

static int
_query_txt(lua_State *L)
{
	app_t *app = lua_touserdata(L, lua_upvalueindex(1));
	int fd;
	int err;
	DNSServiceFlags flags = 0;
	item_t *item = lua_newuserdata(L, sizeof(item_t));
	if(!item)
		goto fail;
	memset(item, 0, sizeof(item_t));
	item->L = L;

	luaL_getmetatable(L, "item_t");
	lua_setmetatable(L, -2);

	lua_pushlightuserdata(L, item);
	lua_pushvalue(L, 2); // callback function
	lua_rawset(L, LUA_REGISTRYINDEX);

#if defined(IF_NAMESIZE)
	lua_getfield(L, 1, "interface");
	const char *if_name = luaL_optstring(L, -1, NULL);
	uint32_t iface = if_name ? if_nametoindex(if_name) : 0;
	lua_pop(L, 1);
#else
	uint32_t iface = 0;
#endif

	lua_getfield(L, 1, "target");
	const char *target = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "fullname");
	item->fullname = luaL_checkstring(L, -1);
	lua_pop(L, 1);

	if((err = DNSServiceQueryRecord(&item->ref, flags, iface, item->fullname, kDNSServiceType_TXT, kDNSServiceClass_IN, _query_txt_cb, item)) != kDNSServiceErr_NoError)
	{
		fprintf(stderr, "_query_txt: dns_sd (%i)\n", err);
		goto fail;
	}
	if((fd = DNSServiceRefSockFD(item->ref)) < 0)
		goto fail;

	item->poll.data = item;
	if((err = uv_poll_init_socket(app->loop, &item->poll, fd)))
	{
		fprintf(stderr, "_query_txt: %s\n", uv_strerror(err));
		goto fail;
	}
	if((err = uv_poll_start(&item->poll, UV_READABLE, _poll_cb)))
	{
		fprintf(stderr, "_query_txt: %s\n", uv_strerror(err));
		goto fail;
	}

	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg ldns_sd [] = {
	{"browse", _browse},
	{"resolve", _resolve},
	{"monitor_ip", _query_ip},
	{"monitor_txt", _query_txt},
	{NULL, NULL}
};

int
luaopen_dns_sd(app_t *app)
{
	lua_State *L = app->L;

#if defined(__linux__)
	// disable annoying warning about using avahi's dns_sd compatibility layer
	setenv("AVAHI_COMPAT_NOWARN", "1", 1);
#endif

	luaL_newmetatable(L, "item_t");
	lua_pushvalue(L, -1);
	lua_setfield(L, -2, "__index");
	lua_pushlightuserdata(L, app);
	luaL_openlib(L, NULL, litem, 1);
	lua_pop(L, 1);

	lua_pushlightuserdata(L, app);
	luaL_openlib(L, "DNS_SD", ldns_sd, 1);

	return 1;
}
