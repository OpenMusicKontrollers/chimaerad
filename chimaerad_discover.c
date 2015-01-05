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

#include <chimaerad.h>

#include <stdlib.h>

static void
cidr_to_subnet(uint32_t *subnet32, uint8_t mask)
{
	uint32_t subn = 0xffffffff;
	if(mask == 0)
		subn = 0x0;
	else
		subn &= ~((1UL << (32-mask)) - 1);
	*subnet32 = subn;
}

static int
str2ipCIDR(const char *str, uint32_t *ip32, uint8_t *mask)
{
	uint16_t sip [4];
	uint16_t smask;
	int res;
	res = sscanf(str, "%hu.%hu.%hu.%hu/%hu",
		sip, sip+1, sip+2, sip+3, &smask) == 5;
	if(res
		&& (sip[0] < 0x100) && (sip[1] < 0x100) && (sip[2] < 0x100) && (sip[3] < 0x100) && (smask <= 0x20) )
	{
		*ip32 = sip[0] << 24;
		*ip32 |= sip[1] << 16;
		*ip32 |= sip[2] << 8;
		*ip32 |= sip[3] << 0;
		*mask = smask;
	}
	return res;
}

chimaerad_source_t *
host_find_source(chimaerad_host_t *host, const char *uid)
{
	chimaerad_source_t *source;
	INLIST_FOREACH(host->sources, source)
		if(!strcmp(source->uid, uid))
			return source;
	
	return NULL;
}

static int
_stream_resolve(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_host_t *host = data;

	static const osc_data_t *msg = (osc_data_t *)"/chimaera/discover\0\0,i\0\0\0\0\0\0";
	osc_stream_send(&host->discover, msg, 28);

	return 1;
}

static int
_success(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_host_t *host = data;

	osc_data_t *ptr = arg;
	int32_t uuid;
	const char *query;

	ptr = osc_get_int32(ptr, &uuid);
	ptr = osc_get_string(ptr, &query);
	
	fprintf(stderr, "success on: %i '%s'\n", uuid, query);

	if(!strcmp(query, "/chimaera/discover"))
	{
		const char *name; 
		const char *uid;
		const char *ip;
		const char *lease;
		const char *reset;
	
		ptr = osc_get_string(ptr, &name);
		ptr = osc_get_string(ptr, &uid);
		ptr = osc_get_string(ptr, &ip);
		ptr = osc_get_string(ptr, &lease);
		ptr = osc_get_string(ptr, &reset);
		
		printf("discovered: %s %s %s %s %s\n", name, uid, ip, lease, reset);

		chimaerad_source_t *source = host_find_source(host, uid);
		if(!source)
		{
			source = calloc(1, sizeof(chimaerad_source_t));
			host->sources = inlist_append(host->sources, INLIST_GET(source));
			source->host = host;

			source->uid = strdup(uid);
			source->name = strdup(name);
			source->ip = strdup(ip);
			source->rate = 2000;

			if(!strcmp(lease, "dhcp"))
				source->lease = CHIMAERAD_SOURCE_LEASE_DHCP;
			else if(!strcmp(lease, "ipv4ll"))
				source->lease = CHIMAERAD_SOURCE_LEASE_IPV4LL;
			else
				source->lease = CHIMAERAD_SOURCE_LEASE_STATIC;

			//FIXME handle reset

			uint8_t mask;
			str2ipCIDR(source->ip, &source->ip4, &mask);
			cidr_to_subnet(&source->mask, mask);
			printf("%08x %08x %08x\n", source->ip4, source->mask, source->ip4 & source->mask);

			source->reachable = 0;
			chimaerad_iface_t *ifa;
			INLIST_FOREACH(host->ifaces, ifa)
				if( (source->ip4 & source->mask) == (ifa->ip4 & ifa->mask) )
				{
					source->iface = ifa;
					source->reachable = 1;
					break;
				}
		}
	}

	return 1;
}

static int
_fail(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_host_t *host = data;
	
	osc_data_t *ptr = arg;
	int32_t uuid;
	const char *query;
	const char *err;

	ptr = osc_get_int32(ptr, &uuid);
	ptr = osc_get_string(ptr, &query);
	ptr = osc_get_string(ptr, &err);

	fprintf(stderr, "failed on: %i '%s' %s\n", uuid, query, err);

	return 1;
}

static const osc_method_t on_osc [] = {
	{"/stream/resolve", "", _stream_resolve},
	{"/success", NULL, _success},
	{"/fail", "iss", _fail},
	{NULL, NULL, NULL},
};

void
chimaerad_discover_recv_cb(osc_stream_t *stream, osc_data_t *buf, size_t len , void *data)
{
	chimaerad_host_t *host = data;

	//printf("chimaerad_discover_recv_cb: %zu %s\n", len, buf);

	if(osc_check_packet(buf, len))
		osc_dispatch_method(0, buf, len, (osc_method_t *)on_osc, NULL, NULL, host);
}

void
chimaerad_discover_send_cb(osc_stream_t *stream, size_t len , void *data)
{
	chimaerad_host_t *host = data;

	//printf("chimaerad_discover_send_cb: %zu\n", len);
	//TODO
}
