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

#include <assert.h>
#include <string.h>
#include <stdlib.h>

static const char *HTTP_200 = "HTTP/1.1 200 OK\r\n";
static const char *HTTP_404 = "HTTP/1.1 404 Not Found\r\n";
static const char *HTTP_HTML = "Content-Type: text/html\r\n\r\n";
static const char *HTTP_JSON = "Content-Type: text/json\r\n\r\n";
static const char *HTTP_CSS = "Content-Type: text/css\r\n\r\n";
static const char *HTTP_JS = "Content-Type: text/javascript\r\n\r\n";
static const char *HTTP_PNG = "Content-Type: image/png\r\n\r\n";
static const char *HTTP_OCTET_STREAM = "Content-Type: application/octet-stream\r\n\r\n";

#if defined(__WINDOWS__)
static inline char *
strndup(const char *s, size_t n)
{
    char *result;
    size_t len = strlen (s);
    if (n < len) len = n;
    result = (char *) malloc (len + 1);
    if (!result) return 0;
    result[len] = '\0';
    return (char *) strncpy (result, s, len);
}
#endif

static void
_on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = malloc(suggested_size); //TODO
	buf->len = suggested_size;
}

static void
_on_client_close(uv_handle_t *handle)
{
	chimaerad_client_t *client = handle->data;
	chimaerad_host_t *host = client->host;

	host->http_clients = inlist_remove(host->http_clients, INLIST_GET(client));
	free(client);
}

void
chimaerad_client_after_write(uv_write_t *req, int status)
{
	uv_tcp_t *handle = (uv_tcp_t *)req->handle;
	chimaerad_client_t *client = handle->data;

	free(client->chunk);
	uv_close((uv_handle_t *)handle, _on_client_close);
}

static void
_on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	uv_tcp_t *handle = (uv_tcp_t *)stream;
	chimaerad_client_t *client = handle->data;
	chimaerad_host_t *host = client->host;

	if(nread >= 0)
	{
		size_t parsed = http_parser_execute(&client->parser, &host->http_settings, buf->base, nread);

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

	free(buf->base);
}

static void
_on_connected(uv_stream_t *handle, int status)
{
	int err;

	chimaerad_host_t *host = handle->data;
	assert(status == 0);

	chimaerad_client_t *client = calloc(1, sizeof(chimaerad_client_t));
	if(!client)
		return;

	client->host = host;
	host->http_clients = inlist_append(host->http_clients, INLIST_GET(client));

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

char *
host_zip_file(chimaerad_host_t *host, const char *path, size_t *size)
{
	struct zip_stat stat;
	if(zip_stat(host->z, path, 0, &stat))
		goto err;
	size_t fsize = stat.size;

	struct zip_file *f = zip_fopen(host->z, path, 0);
	if(!f)
		goto err;
	
	char *str = malloc(fsize + 1);
	if(!str)
		goto err;

	if(zip_fread(f, str, fsize) == -1)
		goto err;
	zip_fclose(f);
	str[fsize] = 0;

	*size = fsize;
	return str;

	err:
		*size = 0;
		if(str)
			free(str);
		if(f)
			zip_fclose(f);
		return NULL;
}

static int
_on_url(http_parser *parser, const char *at, size_t len)
{
	chimaerad_client_t *client = parser->data;
	chimaerad_host_t *host = client->host;

	char *path = strndup(at, len);

	char *json = strchr(path, '?'); // is this a query?
	if(json)
	{
		json++; // skip '?'

		// unescape %20 to '"'
		char *src, *dst;
		for(src=json, dst=json; *src; src++, dst++)
			if(*src == '%')
			{
				*dst = '"';
				src += 2;
			}
			else
				*dst = *src;
		*dst = *src;

		// parse and dispatch JSON
		cJSON *root = cJSON_Parse(json);
		if(root)
		{
			cJSON *query = cJSON_GetObjectItem(root, "query");
			if(query)
				chimaerad_client_dispatch_json(client, query);
			cJSON_Delete(root);
		}
	}
	else // !json
	{
		size_t size;
		if(!strcmp(path, "/"))
		{
			free(path);
			path = strdup("/index.html");
		}
		client->chunk = host_zip_file(host, path+1, &size);
		uv_buf_t msg [3];

		if(client->chunk)
		{
			char *content_type = NULL;
			char *suffix = strrchr(path, '.');

			msg[0].base = (char *)HTTP_200;
			msg[0].len = strlen(HTTP_200);

			if(!suffix || !strcmp(suffix, ".html"))
				content_type = (char *)HTTP_HTML;
			else if(!strcmp(suffix, ".css"))
				content_type = (char *)HTTP_CSS;
			else if(!strcmp(suffix, ".js"))
				content_type = (char *)HTTP_JS;
			else if(!strcmp(suffix, ".png"))
				content_type = (char *)HTTP_PNG;
			else
				content_type = (char *)HTTP_OCTET_STREAM;
			msg[1].base = content_type;

			msg[1].len = strlen(content_type);
			msg[2].base = client->chunk;
			msg[2].len = size;

			uv_write(&client->req, (uv_stream_t *)&client->handle, msg, 3, chimaerad_client_after_write);
		}
		else // not found
		{
			msg[0].base = (char *)HTTP_404;
			msg[0].len = strlen(HTTP_404);

			uv_write(&client->req, (uv_stream_t *)&client->handle, msg, 1, chimaerad_client_after_write);
		}
	}

	free(path);

	return 0;
}

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

static void
_get_interfaces(chimaerad_host_t *host)
{
	uv_interface_address_t *ifaces;
	int count;
	uv_interface_addresses(&ifaces, &count);
	for(int i=0; i<count; i++)
	{
		uv_interface_address_t *iface = &ifaces[i];
		// we are only interested in external IPv4 interfaces
		if( (iface->is_internal == 0) && (iface->address.address4.sin_family == AF_INET) )
		{
			chimaerad_iface_t *ifa = calloc(1, sizeof(chimaerad_iface_t));
			host->ifaces = inlist_append(host->ifaces, INLIST_GET(ifa));

			ifa->name = strdup(ifaces[i].name);
			ifa->ip4 = be32toh(ifaces[i].address.address4.sin_addr.s_addr);
			ifa->mask = be32toh(ifaces[i].netmask.netmask4.sin_addr.s_addr);
			uint8_t mask = subnet_to_cidr(ifa->mask);
			ip2strCIDR(ifa->ip4, mask, ifa->ip);
			printf("=> %i: %s %s %08x %08x %08x\n", i, ifa->name, ifa->ip, ifa->ip4, ifa->mask, ifa->ip4 & ifa->mask);
		}
	}
	uv_free_interface_addresses(ifaces, count);
}

int
chimaerad_host_init(uv_loop_t *loop, chimaerad_host_t *host, uint16_t port)
{
	int err;

	host->z = zip_open("app.zip", ZIP_CHECKCONS, &err);
	if(!host->z)
	{
		fprintf(stderr, "zip_open: %i\n", err);
		return -1;
	}

	_get_interfaces(host);

	host->http_settings.on_url = _on_url;
	host->http_server.data = host;

	if((err = uv_tcp_init(loop, &host->http_server)))
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

	if((err = uv_tcp_bind(&host->http_server, addr, 0)))
	{
		fprintf(stderr, "bind: %s\n", uv_strerror(err));
		return -1;
	}

	if((err = uv_listen((uv_stream_t *)&host->http_server, 128, _on_connected)))
	{
		fprintf(stderr, "listen %s\n", uv_strerror(err));
		return -1;
	}

	// init broadcast discover
	if(osc_stream_init(loop, &host->discover, "osc.udp4://255.255.255.255:4444", chimaerad_discover_recv_cb, chimaerad_discover_send_cb, host))
	{
		fprintf(stderr, "osc_stream_init failed\n");
		return -1;
	}

	// init midi
	//FIXME
	host->midi = rtmidic_out_new(RTMIDIC_API_UNIX_JACK, "ChimaeraD");
	//host->midi = rtmidic_out_new(RTMIDIC_API_LINUX_ALSA, "ChimaeraD");

	return 0;
}

static void
_on_host_close(uv_handle_t *handle)
{
	chimaerad_host_t *host = handle->data;
}

int
chimaerad_host_deinit(chimaerad_host_t *host)
{
	// deinit midi
	rtmidic_out_port_close(host->midi);
	rtmidic_out_free(host->midi);

	// close http clients
	Inlist *l;
	chimaerad_client_t *client;
	INLIST_FOREACH_SAFE(host->http_clients, l, client)
	{
		host->http_clients = inlist_remove(host->http_clients, INLIST_GET(client));
		uv_close((uv_handle_t *)&client->handle, _on_client_close);
	}

	// deinit sources
	chimaerad_source_t *source;
	INLIST_FOREACH_SAFE(host->sources, l, source)
	{
		host->sources = inlist_remove(host->sources, INLIST_GET(source));
		free(source->uid);
		free(source->name);
		free(source->ip);

		chimaerad_source_deinit(source);
		free(source);
	}

	// deinit OSC discover responder
	osc_stream_deinit(&host->discover);

	// deinit http server
	uv_close((uv_handle_t *)&host->http_server, _on_host_close);

	// deinit interfaces
	chimaerad_iface_t *ifa;
	INLIST_FOREACH_SAFE(host->ifaces, l, ifa)
	{
		host->ifaces = inlist_remove(host->ifaces, INLIST_GET(ifa));
		free(ifa->name);
		free(ifa);
	}

	zip_close(host->z);
	
	return 0;
}
