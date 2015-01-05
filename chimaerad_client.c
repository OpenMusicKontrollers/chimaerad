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

static const char *HTTP_200 = "HTTP/1.1 200 OK\r\n";
static const char *HTTP_JSON = "Content-Type: text/json\r\n\r\n";

static int
_client_json_write(chimaerad_client_t *client)
{
	uv_buf_t msg [4];
	msg[0].base = (char *)HTTP_200;
	msg[0].len = strlen(HTTP_200);
	msg[1].base = (char *)HTTP_JSON;
	msg[1].len = strlen(HTTP_JSON);
	msg[2].base = client->chunk;
	msg[2].len = strlen(client->chunk);

	uv_write(&client->req, (uv_stream_t *)&client->handle, msg, 3, chimaerad_client_after_write);

	return 0;
}

static int
_on_json_keepalive(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;

	//printf("/chimaerad/keepalive:\n");

	cJSON *root, *arr;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_interfaces(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;

	printf("/chimaerad/interfaces:\n");

	cJSON *root, *arr;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));
	cJSON_AddItemToObject(root, "reply", arr=cJSON_CreateArray());

	chimaerad_iface_t *ifa;
	INLIST_FOREACH(host->ifaces, ifa)
		cJSON_AddItemToArray(arr, cJSON_CreateString(ifa->ip));

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static void
_serialize_apis(cJSON *reply, chimaerad_host_t *host)
{
	if(rtmidic_has_compiled_api(RTMIDIC_API_MACOSX_CORE))
		cJSON_AddItemToArray(reply, cJSON_CreateString("MACOSX_CORE"));
	if(rtmidic_has_compiled_api(RTMIDIC_API_LINUX_ALSA))
		cJSON_AddItemToArray(reply, cJSON_CreateString("LINUX_ALSA"));
	if(rtmidic_has_compiled_api(RTMIDIC_API_UNIX_JACK))
		cJSON_AddItemToArray(reply, cJSON_CreateString("UNIX_JACK"));
	if(rtmidic_has_compiled_api(RTMIDIC_API_WINDOWS_MM))
		cJSON_AddItemToArray(reply, cJSON_CreateString("WINDOWS_MM"));
}

static void
_serialize_ports(cJSON *reply, chimaerad_host_t *host)
{
	unsigned int count = rtmidic_out_port_count(host->midi);
	for(unsigned int i=0; i<count; i++)
	{
		char *name = strdup(rtmidic_out_port_name(host->midi, i));
		cJSON_AddItemToArray(reply, cJSON_CreateString(name));
		free(name);
	}
}

static int
_on_json_apis(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;

	printf("/chimaerad/apis:\n");

	cJSON *root, *arr;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));
	cJSON_AddItemToObject(root, "reply", arr=cJSON_CreateArray());

	_serialize_apis(arr, host);

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_ports(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;

	printf("/chimaerad/ports:\n");

	cJSON *root, *arr;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));
	cJSON_AddItemToObject(root, "reply", arr=cJSON_CreateArray());

	_serialize_ports(arr, host);

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_sources(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;

	printf("/chimaerad/sources:\n");

	cJSON *root, *arr;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));
	cJSON_AddItemToObject(root, "reply", arr=cJSON_CreateArray());

	chimaerad_source_t *source;
	INLIST_FOREACH(host->sources, source)
		cJSON_AddItemToArray(arr, cJSON_CreateString(source->uid));

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static void
_serialize_source(cJSON *reply, chimaerad_source_t *source)
{
	// comm
	cJSON_AddItemToObject(reply, "uid", cJSON_CreateString(source->uid));
	cJSON_AddItemToObject(reply, "name", cJSON_CreateString(source->name));
	cJSON_AddItemToObject(reply, "lease", cJSON_CreateString(source->lease == CHIMAERAD_SOURCE_LEASE_DHCP ? "dhcp" : (source->lease == CHIMAERAD_SOURCE_LEASE_IPV4LL ? "ipv4ll" : "static")));
	cJSON_AddItemToObject(reply, "ip", cJSON_CreateString(source->ip));
	cJSON_AddItemToObject(reply, "reachable", cJSON_CreateBool(source->reachable));

	// claim
	cJSON_AddItemToObject(reply, "claimed", cJSON_CreateBool(source->claimed));
	
	// conf
	cJSON_AddItemToObject(reply, "mode", cJSON_CreateString(source->mode == CHIMAERAD_SOURCE_MODE_UDP ? "udp" : "tcp"));
	cJSON_AddItemToObject(reply, "rate", cJSON_CreateNumber(source->rate));
	cJSON_AddItemToObject(reply, "sync", cJSON_CreateString("none")); //FIXME
	//TODO
}

static int
_on_json_source_info(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;
	char *uid = cJSON_GetObjectItem(obj, "uid")->valuestring;

	printf("/chimaerad/source/info: %s\n", uid);

	cJSON *root;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));

	chimaerad_source_t *source = host_find_source(host, uid);
	if(source)
	{
		cJSON *reply;
		cJSON_AddItemToObject(root, "reply", reply=cJSON_CreateObject());
		_serialize_source(reply, source);
	}

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_source_com(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;
	char *uid = cJSON_GetObjectItem(obj, "uid")->valuestring;
	char *name = cJSON_GetObjectItem(obj, "name")->valuestring;
	char *mode = cJSON_GetObjectItem(obj, "lease")->valuestring;
	char *ip = cJSON_GetObjectItem(obj, "ip")->valuestring;

	printf("/chimaerad/source/com: %s\n", uid);

	chimaerad_source_t *source = host_find_source(host, uid);
	if(!source)
		return -1;

	chimaerad_source_lease_t lease;
	if(!strcmp(mode, "dhcp"))
		lease = CHIMAERAD_SOURCE_LEASE_DHCP;
	else if(!strcmp(mode, "ipv4ll"))
		lease = CHIMAERAD_SOURCE_LEASE_IPV4LL;
	else
		lease = CHIMAERAD_SOURCE_LEASE_STATIC;

	static osc_data_t buf [512];
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + 512;
	osc_data_t *bndl;
	int dirty = 0;
	
	ptr = osc_start_bundle(ptr, end, OSC_IMMEDIATE, &bndl);

	if((dirty = dirty || strcmp(name, source->name)))
	{
		free(source->name);
		source->name = strdup(name);
		ptr = osc_set_bundle_item(ptr, end, "/info/name", "is", 11, source->name);
	}

	if((dirty = dirty || (lease != source->lease)))
	{
		switch(source->lease)
		{
			case CHIMAERAD_SOURCE_LEASE_DHCP:
				ptr = osc_set_bundle_item(ptr, end, "/dhcp/enabled", "ii", 12, 0);
				// fall-through

			case CHIMAERAD_SOURCE_LEASE_IPV4LL:
				ptr = osc_set_bundle_item(ptr, end, "/ipv4/enabled", "ii", 13, 0);
				break;

			case CHIMAERAD_SOURCE_LEASE_STATIC:
				break;
		}

		source->lease = lease;
		switch(source->lease)
		{
			case CHIMAERAD_SOURCE_LEASE_DHCP:
				ptr = osc_set_bundle_item(ptr, end, "/dhcp/enabled", "ii", 14, 1);
				// fall-through

			case CHIMAERAD_SOURCE_LEASE_IPV4LL:
				ptr = osc_set_bundle_item(ptr, end, "/ipv4ll/enabled", "ii", 15, 1);
				break;

			case CHIMAERAD_SOURCE_LEASE_STATIC:
				ptr = osc_set_bundle_item(ptr, end, "/comm/ip", "is", 16, "192.168.1.177/24");
				break;
		}
	}

	if(dirty)
	{
		ptr = osc_set_bundle_item(ptr, end, "/config/save", "i", 17);
		ptr = osc_set_bundle_item(ptr, end, "/reset/soft", "i", 18); //FIXME in firmware
		ptr = osc_end_bundle(ptr, end, bndl);
		osc_stream_send(&host->discover, buf, ptr - buf);
	}
	else
		printf("not dirty\n");

	cJSON *root;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));

	if(source)
	{
		cJSON *reply;
		cJSON_AddItemToObject(root, "reply", reply=cJSON_CreateObject());
		_serialize_source(reply, source);
	}

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_source_claim(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;
	char *uid = cJSON_GetObjectItem(obj, "uid")->valuestring;
	int claimed = cJSON_GetObjectItem(obj, "claimed")->valueint;

	printf("/chimaerad/claim: %s %i\n", uid, claimed);

	chimaerad_source_t *source = host_find_source(host, uid);
	if(!source)
		return -1;

	if(claimed != source->claimed)
	{
		if(source->claimed)
			chimaerad_source_deinit(source);
		else // !source->claimed
			chimaerad_source_init(host->http_server.loop, source);
		source->claimed = claimed;
	}

	cJSON *root;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));

	if(source)
	{
		cJSON *reply;
		cJSON_AddItemToObject(root, "reply", reply=cJSON_CreateObject());
		_serialize_source(reply, source);
	}

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_source_conf(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;
	char *uid = cJSON_GetObjectItem(obj, "uid")->valuestring;
	char *mode_s = cJSON_GetObjectItem(obj, "mode")->valuestring;
	int rate = cJSON_GetObjectItem(obj, "rate")->valueint;
	char *sync_s = cJSON_GetObjectItem(obj, "sync")->valuestring;

	printf("/chimaerad/conf: %s %s %i %s\n", uid, mode_s, rate, sync_s);

	chimaerad_source_t *source = host_find_source(host, uid);
	if(!source)
		return -1;

	chimaerad_source_mode_t mode = !strcmp(mode_s, "udp") ? CHIMAERAD_SOURCE_MODE_UDP : CHIMAERAD_SOURCE_MODE_TCP;
	if( (source->mode != mode) || (source->rate != rate) )
	{
		chimaerad_source_deinit(source);
		source->mode = mode;
		source->rate = rate;
		//TODO sync
		chimaerad_source_init(host->http_server.loop, source);
	}

	cJSON *root;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(1));

	if(source)
	{
		cJSON *reply;
		cJSON_AddItemToObject(root, "reply", reply=cJSON_CreateObject());
		_serialize_source(reply, source);
	}

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_api_claim(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;
	char *name = cJSON_GetObjectItem(obj, "name")->valuestring;

	printf("/chimaerad/api/claim: %s\n", name);

	RtMidiC_API api = RTMIDIC_API_UNSPECIFIED;
	if(!strcmp(name, "MACOSX_CORE"))
		api = RTMIDIC_API_MACOSX_CORE;
	else if(!strcmp(name, "LINUX_ALSA"))
		api = RTMIDIC_API_LINUX_ALSA;
	else if(!strcmp(name, "UNIX_JACK"))
		api = RTMIDIC_API_UNIX_JACK;
	else if(!strcmp(name, "WINDOWS_MM"))
		api = RTMIDIC_API_WINDOWS_MM;

	rtmidic_out_port_close(host->midi);
	rtmidic_out_free(host->midi);
	host->midi = rtmidic_out_new(api, "ChimaeraD");

	cJSON *root;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(host->midi ? 1 : 0));

	cJSON *reply;
	cJSON_AddItemToObject(root, "reply", reply=cJSON_CreateArray());
	_serialize_apis(reply, host);

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static int
_on_json_port_claim(chimaerad_client_t *client, cJSON *obj)
{
	chimaerad_host_t *host = client->host;
	int pos = cJSON_GetObjectItem(obj, "pos")->valueint;

	printf("/chimaerad/port/claim: %i\n", pos);

	int res;

	rtmidic_out_port_close(host->midi);
	if(pos == -1)
		res = rtmidic_out_virtual_port_open(host->midi, "midi_out");
	else
		res = rtmidic_out_port_open(host->midi, pos, rtmidic_out_port_name(host->midi, pos));

	cJSON *root;
	root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "success", cJSON_CreateBool(res == 0 ? 1 : 0));

	cJSON *reply;
	cJSON_AddItemToObject(root, "reply", reply=cJSON_CreateObject());
	_serialize_ports(reply, host);

	client->chunk = cJSON_Print(root);
	cJSON_Delete(root);
	return _client_json_write(client);
}

static const chimaerad_method_t methods [] = {
	{"/chimaerad/keepalive", _on_json_keepalive},

	{"/chimaerad/interfaces", _on_json_interfaces},
	{"/chimaerad/sources", _on_json_sources},
	{"/chimaerad/apis", _on_json_apis},
	{"/chimaerad/ports", _on_json_ports},

	{"/chimaerad/source/info",  _on_json_source_info},
	{"/chimaerad/source/com",  _on_json_source_com},
	{"/chimaerad/source/claim",  _on_json_source_claim},
	{"/chimaerad/source/conf",  _on_json_source_conf},
	
	{"/chimaerad/api/claim",  _on_json_api_claim},
	{"/chimaerad/port/claim",  _on_json_port_claim},

	{NULL, NULL}	
};

int
chimaerad_client_dispatch_json(chimaerad_client_t *client, cJSON *query)
{
	cJSON *obj = query->child; //TODO there may be more than one
	char *path = obj->string;

	for(const chimaerad_method_t *method=methods; method->cb; method++)
		if(!method->path || !strcmp(method->path, path))
				if(method->cb) return method->cb(client, obj);

	return -1;
}
