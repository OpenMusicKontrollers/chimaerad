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

#include <string.h>

#include <chimaerad.h>

#include <lua.h>
#include <lauxlib.h>

#include <uv.h>

static int
_list(lua_State *L)
{
	uv_interface_address_t *ifaces;
	int count;
	int err;
	uv_interface_addresses(&ifaces, &count);
	lua_createtable(L, count, 0);

	for(int i=0; i<count; i++)
	{
		uv_interface_address_t *iface = &ifaces[i];
		char buffer[INET6_ADDRSTRLEN];
		char hmac [18]; // "xx:xx:xx:xx:xx:xx\0"

		lua_createtable(L, 0, 6);
		{
			lua_pushstring(L, iface->name);
			lua_setfield(L, -2, "name");

			lua_pushboolean(L, iface->is_internal);
			lua_setfield(L, -2, "internal");

			if(iface->address.address4.sin_family == AF_INET)
				err = uv_inet_ntop(AF_INET, &iface->address.address4.sin_addr.s_addr, buffer, sizeof(buffer));
			else
				err = uv_inet_ntop(AF_INET6, &iface->address.address6.sin6_addr.s6_addr, buffer, sizeof(buffer));
			if(!err)
			{
				lua_pushstring(L, buffer);
				lua_setfield(L, -2, "address");
			}

			if(iface->netmask.netmask4.sin_family == AF_INET)
				lua_pushstring(L, "inet");
			else
				lua_pushstring(L, "inet6");
			lua_setfield(L, -2, "version");

			if(iface->netmask.netmask4.sin_family == AF_INET)
				err = uv_inet_ntop(AF_INET, &iface->netmask.netmask4.sin_addr.s_addr, buffer, sizeof(buffer));
			else
				err = uv_inet_ntop(AF_INET6, &iface->netmask.netmask6.sin6_addr.s6_addr, buffer, sizeof(buffer));
			if(!err)
			{
				lua_pushstring(L, buffer);
				lua_setfield(L, -2, "netmask");
			}

			uint8_t *mac = (uint8_t *)iface->phys_addr;
			sprintf(hmac, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
			lua_pushstring(L, hmac);
			lua_setfield(L, -2, "mac");
		}

		lua_rawseti(L, -2, i+1);
	}
	uv_free_interface_addresses(ifaces, count);

	return 1;
}

static int
_check(lua_State *L)
{
	const char *af = luaL_checkstring(L, 1);
	const char *src_ip = luaL_checkstring(L, 2);
	const char *src_mask = luaL_checkstring(L, 3);
	const char *dst_ip = luaL_checkstring(L, 4);

	int AF = 0;
	if(!strcmp(af, "inet"))
		AF = AF_INET;
	else if(!strcmp(af, "inet6"))
		AF = AF_INET6;

	int ret = 0;

	if(AF == AF_INET)
	{
		uint32_t src_ip4;
		uint32_t src_mask4;
		uint32_t dst_ip4;

		if(uv_inet_pton(AF_INET, src_ip, &src_ip4)) goto fail;
		if(uv_inet_pton(AF_INET, src_mask, &src_mask4)) goto fail;
		if(uv_inet_pton(AF_INET, dst_ip, &dst_ip4)) goto fail;

		ret = (src_ip4 & src_mask4) == (dst_ip4 & src_mask4);
	}
	else
	{
		uint16_t src_ip6 [8];
		uint16_t src_mask6 [8];
		uint16_t dst_ip6 [8];

		if(uv_inet_pton(AF_INET6, src_ip, src_ip6)) goto fail;
		if(uv_inet_pton(AF_INET6, src_mask, src_mask6)) goto fail;
		if(uv_inet_pton(AF_INET6, dst_ip, dst_ip6)) goto fail;

		ret =  (src_ip6[0] & src_mask6[0]) == (dst_ip6[0] & src_mask6[0])
				&& (src_ip6[1] & src_mask6[1]) == (dst_ip6[1] & src_mask6[1])
				&& (src_ip6[2] & src_mask6[2]) == (dst_ip6[2] & src_mask6[2])
				&& (src_ip6[3] & src_mask6[3]) == (dst_ip6[3] & src_mask6[3])
				&& (src_ip6[4] & src_mask6[4]) == (dst_ip6[4] & src_mask6[4])
				&& (src_ip6[5] & src_mask6[5]) == (dst_ip6[5] & src_mask6[5])
				&& (src_ip6[6] & src_mask6[6]) == (dst_ip6[6] & src_mask6[6])
				&& (src_ip6[7] & src_mask6[7]) == (dst_ip6[7] & src_mask6[7]);
	}

	lua_pushboolean(L, ret);
	return 1;

fail:
	lua_pushnil(L);
	return 1;
}

static const luaL_Reg liface [] = {
	{"list", _list},
	{"check", _check},
	{NULL, NULL}
};

int
luaopen_iface(app_t *app)
{
	lua_State *L = app->L;

	lua_newtable(L);
	lua_pushlightuserdata(L, app);
	luaL_setfuncs(L, liface, 1);
	lua_setglobal(L, "IFACE");

	return 0;
}
