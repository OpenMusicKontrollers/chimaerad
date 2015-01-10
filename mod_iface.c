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

#define LUA_COMPAT_MODULE
#include <lua.h>
#include <lauxlib.h>

#include <uv.h>

static void
ip2strCIDR(uint32_t ip32, uint8_t mask, char *str)
{
	uint8_t ip [4];
	ip[0] = (ip32 >> 24) & 0xff;
	ip[1] = (ip32 >> 16) & 0xff;
	ip[2] = (ip32 >> 8) & 0xff;
	ip[3] = (ip32 >> 0) & 0xff;
	sprintf(str, "%hhu.%hhu.%hhu.%hhu/%hhu",
		ip[0], ip[1], ip[2], ip[3], mask);
}

static uint8_t
subnet_to_cidr(uint32_t subnet32)
{
	uint8_t mask;
	if(subnet32 == 0)
		return 0;
	for(mask=0; !(subnet32 & 1); mask++)
		subnet32 = subnet32 >> 1;
	return 32 - mask;
}

// JSON -> Lua
static int
_list(lua_State *L)
{
	uv_interface_address_t *ifaces;
	int count;
	uv_interface_addresses(&ifaces, &count);
	lua_createtable(L, 0, count);

	for(int i=0; i<count; i++)
	{
		uv_interface_address_t *iface = &ifaces[i];
		// we are only interested in external IPv4 interfaces
		if( (iface->is_internal == 0) && (iface->address.address4.sin_family == AF_INET) )
		{
			uint32_t ip = be32toh(ifaces[i].address.address4.sin_addr.s_addr);
			uint32_t sub = be32toh(ifaces[i].netmask.netmask4.sin_addr.s_addr);

			char cidr [20];
			uint8_t mask = subnet_to_cidr(sub);
			ip2strCIDR(ip, mask, cidr);

			lua_pushstring(L, cidr);
			lua_setfield(L, -2, ifaces[i].name);
		}
	}
	uv_free_interface_addresses(ifaces, count);

	return 1;
}

static const luaL_Reg liface [] = {
	{"list", _list},
	{NULL, NULL}
};

int
luaopen_iface(lua_State *L)
{
	luaL_register(L, "IFACE", liface);		

	return 1;
}
