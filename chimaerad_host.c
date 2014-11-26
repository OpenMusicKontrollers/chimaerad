#include <chimaerad.h>

#include <assert.h>
#include <string.h>

static const char *HTTP_200 = "HTTP/1.1 200 OK\r\n";
static const char *HTTP_404 = "HTTP/1.1 404 Not Found\r\n";
static const char *HTTP_HTML = "Content-Type: text/html\r\n\r\n";
static const char *HTTP_CSS = "Content-Type: text/css\r\n\r\n";
static const char *HTTP_JS = "Content-Type: text/javascript\r\n\r\n";

static void
_on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
//printf("_on_alloc\n");
	buf->base = malloc(suggested_size); //TODO
	buf->len = suggested_size;
}

static void
_on_client_close(uv_handle_t *handle)
{
	chimaerad_client_t *client = handle->data;
	chimaerad_host_t *host = client->host;

	host->http_clients = eina_inlist_remove(host->http_clients, EINA_INLIST_GET(client));
	free(client);
}

static void
_on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
//printf("_on_read\n");
	uv_tcp_t *handle = (uv_tcp_t *)stream;
	chimaerad_client_t *client = handle->data;
	chimaerad_host_t *host = client->host;
	size_t parsed;

	if(nread >= 0)
	{
		parsed = http_parser_execute(&client->parser, &host->http_settings, buf->base, nread);

		if(parsed < nread)
		{
			fprintf(stderr, "parse error\n");
			uv_close((uv_handle_t *)handle, _on_client_close);
		}
	}
	else
	{
		uv_errno_t err = nread;

		if(err == UV_EOF)
			;
		else
			fprintf(stderr, "read: %s\n", uv_strerror(err));

		uv_close((uv_handle_t *)handle, _on_client_close);
	}

	free(buf->base);
}

static void
_on_connected(uv_stream_t *handle, int status)
{
//printf("_on_connected\n");
	chimaerad_host_t *host = handle->data;
	assert(status == 0);

	chimaerad_client_t *client = calloc(1, sizeof(chimaerad_client_t));
	if(!client)
		return;

	client->host = host;
	host->http_clients = eina_inlist_append(host->http_clients, EINA_INLIST_GET(client));

	int r = uv_tcp_init(handle->loop, &client->handle);	
	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "uv_tcp_init: %s\n", uv_strerror(err));
		return;
	}

	r = uv_accept(handle, (uv_stream_t *)&client->handle);
	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "uv_accept: %s\n", uv_strerror(err));
		return;
	}

	client->handle.data = client;
	client->parser.data = client;

	http_parser_init(&client->parser, HTTP_REQUEST);

	uv_read_start((uv_stream_t *)&client->handle, _on_alloc, _on_read);
}

static void
_after_write(uv_write_t *req, int status)
{
//printf("_after_write\n");
	uv_tcp_t *handle = (uv_tcp_t *)req->handle;
	chimaerad_client_t *client = handle->data;

	free(client->chunk);

	uv_close((uv_handle_t *)handle, _on_client_close);
}

static int
_ping(chimaerad_client_t *client, cJSON *osc)
{
	int i = cJSON_GetArrayItem(osc, 2)->valueint;
	float f = cJSON_GetArrayItem(osc, 3)->valuedouble;
	char *s = cJSON_GetArrayItem(osc, 4)->valuestring;

	printf("/ping: %i %f '%s'\n", i, f, s);
	
	client->chunk = strdup("<b>pong</b> <i>pong</i>");

	uv_buf_t msg [4];
	msg[0].base = (char *)HTTP_200;
	msg[0].len = strlen(HTTP_200);
	msg[1].base = (char *)HTTP_HTML;
	msg[1].len = strlen(HTTP_HTML);
	msg[2].base = client->chunk;
	msg[2].len = strlen(client->chunk);

	uv_write(&client->req, (uv_stream_t *)&client->handle, msg, 3, _after_write);

	return 0;
}

const chimaerad_method_t responder [] = {
	{"/ping", "ifs", _ping},
	{NULL, NULL, NULL}	
};

static int
_json_osc_dispatch(chimaerad_client_t *client, cJSON *osc, const chimaerad_method_t *methods)
{
	char *path = cJSON_GetArrayItem(osc, 0)->valuestring;
	char *fmt = cJSON_GetArrayItem(osc, 1)->valuestring;

	for(const chimaerad_method_t *method=methods; method->cb; method++)
		if(!method->path || !strcmp(method->path, path))
			if(!method->fmt || !strcmp(method->fmt, fmt))
				if(method->cb)
					return method->cb(client, osc);

	return -1;
}

static int
_on_url(http_parser *parser, const char *at, size_t len)
{
	chimaerad_client_t *client = parser->data;
	chimaerad_host_t *host = client->host;
	lua_State *L = host->L;

	char *path = strndup(at, len);

	char *json = strchr(path, '?');
	if(json)
	{
		json++;

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

		printf("has JSON: %s\n", json);

		cJSON *root = cJSON_Parse(json);
		if(root)
		{
			cJSON *osc = cJSON_GetObjectItem(root, "osc");
			if(osc)
				_json_osc_dispatch(client, osc, responder);
			cJSON_Delete(root);
		}
	}
	else // !json
	{
		int size;
		client->chunk = eet_read(host->eet, path, &size);
		uv_buf_t msg [3];
		
		printf("no JSON: %s\n", path);

		if(client->chunk)
		{
			printf("_on_url: %s %i\n", path, size);

			msg[0].base = (char *)HTTP_200;
			msg[0].len = strlen(HTTP_200);
			char *content_type = NULL;
			char *suffix = strrchr(path, '.');
			if(!suffix || !strcmp(suffix, ".html"))
				content_type = (char *)HTTP_HTML;
			else if(!strcmp(suffix, ".css"))
				content_type = (char *)HTTP_CSS;
			else if(!strcmp(suffix, ".js"))
				content_type = (char *)HTTP_JS;
			else
				content_type = (char *)HTTP_HTML;
			msg[1].base = content_type;
			msg[1].len = strlen(content_type);
			msg[2].base = client->chunk;
			msg[2].len = size;

			uv_write(&client->req, (uv_stream_t *)&client->handle, msg, 3, _after_write);
		}
		else
		{
			printf("404\n");

			msg[0].base = (char *)HTTP_404;
			msg[0].len = strlen(HTTP_404);

			uv_write(&client->req, (uv_stream_t *)&client->handle, msg, 1, _after_write);
		}
	}

	free(path);

	return 0;
}

static void
_poll_cb(uv_poll_t* handle, int status, int events)
{
	chimaerad_sd_t *sd = handle->data;
	DNSServiceProcessResult(sd->ref);
}


static void
_on_service_query_record_reply(
	DNSServiceRef ref,
	DNSServiceFlags flags,
	uint32_t interface,
	DNSServiceErrorType err_code,
	const char *full_name,
	uint16_t rrtype,
	uint16_t rrclass,
	uint16_t rdlen,
	const void *rdata,
	uint32_t ttl,
	void *data)
{
	chimaerad_host_t *host = data;

	printf("_service_query_record_reply: %s \n", full_name);
	//TODO
}

static void
_on_service_resolve_reply(
	DNSServiceRef ref,
	DNSServiceFlags flags,
	uint32_t interface,
	DNSServiceErrorType err_code,
	const char *full_name,
	const char *host_target,
	uint16_t port,
	uint16_t txt_len,
	const unsigned char *txt_record,
	void *data)
{
	chimaerad_host_t *host = data;

	printf("_service_resolve_reply: %s %s %hu\n", full_name, host_target, port);
	//TODO
}

static void
_on_service_browse_reply(
	DNSServiceRef ref,
	DNSServiceFlags flags,
	uint32_t interface,
	DNSServiceErrorType err_code,
	const char *service_name,
	const char *reg_type,
	const char *reply_domain,
	void *data
)
{
	chimaerad_host_t *host = data;

	printf("%i %s %s %s\n", interface, service_name, reg_type, reply_domain);
	//TODO

	if(!strcmp(service_name, "chimaera"))
	{
		chimaerad_sd_t *resolve = calloc(1, sizeof(chimaerad_sd_t));
		chimaerad_sd_t *query = calloc(1, sizeof(chimaerad_sd_t));
		int fd;

		host->sd_resolves = eina_inlist_append(host->sd_resolves, EINA_INLIST_GET(resolve));
		host->sd_queries = eina_inlist_append(host->sd_queries, EINA_INLIST_GET(query));

		DNSServiceResolve(&resolve->ref, flags, interface, service_name, reg_type, reply_domain, _on_service_resolve_reply, host);
		DNSServiceQueryRecord(&query->ref, flags, interface, service_name, kDNSServiceType_PTR, kDNSServiceClass_IN, _on_service_query_record_reply, host);

		fd = DNSServiceRefSockFD(resolve->ref);
		printf("fd: %i\n", fd);
		//resolve->handle.data = resolve;
		//uv_poll_init(host->http_server.loop, &resolve->handle, fd);
		//uv_poll_start(&resolve->handle, UV_READABLE, _poll_cb);

		fd = DNSServiceRefSockFD(query->ref);
		printf("fd: %i\n", fd);
		//query->handle.data = query;
		//uv_poll_init(host->http_server.loop, &query->handle, fd);
		//uv_poll_start(&query->handle, UV_READABLE, _poll_cb);
	}
}

int
_eet_loader(lua_State *L)
{
	chimaerad_host_t *host = lua_touserdata(L, lua_upvalueindex(1));
	const char *module = luaL_checkstring(L, 1);

	char key [256];
	sprintf(key, "/%s.lua", module);

	int size;
	char *chunk = eet_read(host->eet, key, &size);
	if(chunk)
	{
		printf("_eet_loader: %s %i\n", key, size);
		luaL_loadbuffer(L, chunk, size, module);
		free(chunk);
		return 1;
	}

	return 0;
}

int
_eet_get(lua_State *L)
{
	chimaerad_host_t *host = lua_touserdata(L, lua_upvalueindex(1));
	const char *key = luaL_checkstring(L, 1);
		
	printf("_eet_get: %s\n", key);

	int size;
	char *chunk = eet_read(host->eet, key, &size);
	if(chunk)
	{
		printf("_eet_get: %s %i\n", key, size);
		lua_pushlstring(L, chunk, size);
		free(chunk);
	}
	else
		lua_pushnil(L);

	return 1;
}

int
chimaerad_host_init(uv_loop_t *loop, chimaerad_host_t *host, uint16_t port)
{
	int r;
	
	if(!loop || !host)
		return -1;

	if(!(host->L = lua_open()))
		return -1;
	luaL_openlibs(host->L);

	// overwrite loader functions with our own
	lua_getglobal(host->L, "package");
	{
		lua_newtable(host->L);
		{
			lua_pushlightuserdata(host->L, host);
			lua_pushcclosure(host->L, _eet_loader, 1);
			lua_rawseti(host->L, -2, 1);
		}
		lua_setfield(host->L, -2, "loaders");
	}
	lua_pop(host->L, 1); // package

	// register eet loader function
	lua_pushlightuserdata(host->L, host);
	lua_pushcclosure(host->L, _eet_get, 1);
	lua_setglobal(host->L, "eet");

	host->eet = eet_open("chimaerad.eet", EET_FILE_MODE_READ);

	/* FIXME
	if(luaL_dostring(host->L, "require('chimaerad_http');"))
	{
		fprintf(stderr, "err: %s\n", lua_tostring(host->L, -1));
		return -1;
	}
	*/

	host->http_settings.on_url = _on_url;

	host->http_server.data = host;
	r = uv_tcp_init(loop, &host->http_server);
	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "uv_tcp_init: %s\n", uv_strerror(err));
		return -1;
	}

	struct sockaddr_in addr_ip4;
	struct sockaddr *addr = (struct sockaddr *)&addr_ip4;

	r = uv_ip4_addr("0.0.0.0", port, &addr_ip4);
	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "uv_ip4_addr: %s\n", uv_strerror(err));
		return -1;
	}

	r = uv_tcp_bind(&host->http_server, addr, 0);
	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "bind: %s\n", uv_strerror(err));
		return -1;
	}

	r = uv_listen((uv_stream_t *)&host->http_server, 128, _on_connected);
	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "listen %s\n", uv_strerror(err));
		return -1;
	}

	putenv("AVAHI_COMPAT_NOWARN=1");
	chimaerad_sd_t *osc_udp = calloc(1, sizeof(chimaerad_sd_t));
	chimaerad_sd_t *osc_tcp = calloc(1, sizeof(chimaerad_sd_t));
	chimaerad_sd_t *ntp_udp = calloc(1, sizeof(chimaerad_sd_t));
	chimaerad_sd_t *ptp_udp = calloc(1, sizeof(chimaerad_sd_t));

	host->sd_browses = eina_inlist_append(host->sd_browses, EINA_INLIST_GET(osc_udp));
	host->sd_browses = eina_inlist_append(host->sd_browses, EINA_INLIST_GET(osc_tcp));
	host->sd_browses = eina_inlist_append(host->sd_browses, EINA_INLIST_GET(ntp_udp));
	host->sd_browses = eina_inlist_append(host->sd_browses, EINA_INLIST_GET(ptp_udp));

	DNSServiceBrowse(&osc_udp->ref, 0, 0, "_osc._udp", NULL, _on_service_browse_reply, host);
	DNSServiceBrowse(&osc_tcp->ref, 0, 0, "_osc._tcp", NULL, _on_service_browse_reply, host);
	DNSServiceBrowse(&ntp_udp->ref, 0, 0, "_ntp._udp", NULL, _on_service_browse_reply, host);
	DNSServiceBrowse(&ptp_udp->ref, 0, 0, "_ptp._udp", NULL, _on_service_browse_reply, host);

	chimaerad_sd_t *sd;
	EINA_INLIST_FOREACH(host->sd_browses, sd)
	{
		int fd = DNSServiceRefSockFD(sd->ref);
		printf("fd: %i\n", fd);
		sd->handle.data = sd;
		uv_poll_init(loop, &sd->handle, fd);
		uv_poll_start(&sd->handle, UV_READABLE, _poll_cb);
	}

	return 0;
}

static void
_on_host_close(uv_handle_t *handle)
{
	chimaerad_host_t *host = handle->data;

	chimaerad_sd_t *sd;
	EINA_INLIST_FREE(host->sd_browses, sd)
	{
		uv_poll_stop(&sd->handle);
		DNSServiceRefDeallocate(sd->ref);
		free(sd);
	}

	if(host->L)
		lua_close(host->L);

	eet_close(host->eet);
}

int
chimaerad_host_deinit(chimaerad_host_t *host)
{
	chimaerad_client_t *client;
	EINA_INLIST_FREE(host->http_clients, client)
		uv_close((uv_handle_t *)&client->handle, _on_client_close);

	uv_close((uv_handle_t *)&host->http_server, _on_host_close);
	
	return 0;
}
