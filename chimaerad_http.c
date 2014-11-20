#include <chimaerad.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// include http_parser
#include <http_parser.h>

#define RESPONSE \
	"HTTP/1.1 200 OK\r\n" \
	"Content-Type: text/html\r\n" \
	"\r\n" \
	"<!DOCTYPE html>" \
	"<html>" \
	"<body>" \
	"<h1>Chimaera<span style=\"color:#c00;\">D</span></h1>" \
	"</body>" \
	"</html>"

static uv_buf_t resbuf;
static uv_tcp_t server;
static http_parser_settings settings;

typedef struct {
	uv_tcp_t handle;
	http_parser parser;
	uv_write_t write_req;
} client_t;

static void
on_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
//printf("on_alloc\n");
	buf->base = malloc(suggested_size);
	buf->len = suggested_size;
}

static void
on_read(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
//printf("on_read\n");
	uv_tcp_t *handle = (uv_tcp_t *)stream;
	client_t *client = handle->data;
	size_t parsed;

	if(nread >= 0)
	{
		parsed = http_parser_execute(&client->parser, &settings, buf->base, nread);

		if(parsed < nread)
		{
			fprintf(stderr, "parse error\n");
			uv_close((uv_handle_t *)handle, NULL);
			free(client);
		}
	}
	else
	{
		uv_errno_t err = nread;

		if(err == UV_EOF)
			;
		else
			fprintf(stderr, "read: %s\n", uv_strerror(err));

		uv_close((uv_handle_t *)handle, NULL);
		free(client);
	}

	free(buf->base);
}

static void
on_connected(uv_stream_t *stream, int status)
{
//printf("on_connected\n");
	uv_tcp_t *handle = (uv_tcp_t *)stream;
	assert(handle == &server);
	assert(status == 0);

	client_t *client = malloc(sizeof(client_t));
	if(!client)
		return;

	int r = uv_tcp_init(handle->loop, &client->handle);	
	r = uv_accept((uv_stream_t *)&server, (uv_stream_t *)&client->handle);

	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "accept: %s\n", uv_strerror(err));
		return;
	}

	client->handle.data = client;
	client->parser.data = client;

	http_parser_init(&client->parser, HTTP_REQUEST);

	uv_read_start((uv_stream_t *)&client->handle, on_alloc, on_read);
}

static void
after_write(uv_write_t *req, int status)
{
//printf("after_write\n");
	uv_tcp_t *handle = (uv_tcp_t *)req->handle;
	client_t *client = handle->data;;

	uv_close((uv_handle_t *)handle, NULL);
	free(client);
}

static int
on_headers_complete(http_parser *parser)
{
//printf("on_headers_complete\n");
	client_t *client = parser->data;

	uv_write(&client->write_req, (uv_stream_t *)&client->handle, &resbuf, 1, after_write);

	return 0;
}

static int
on_url(http_parser *parser, const char *at, size_t len)
{
	printf("on_url: %.*s\n", (int)len, at);

	return 0;
}

int
chimaerad_http_init(uv_loop_t *loop, uint16_t port)
{
	resbuf.base = RESPONSE;
	resbuf.len = sizeof(RESPONSE);

	settings.on_headers_complete = on_headers_complete;
	settings.on_url = on_url;

	uv_tcp_init(loop, &server);

	struct sockaddr_in addr_ip4;
	struct sockaddr *addr = (struct sockaddr *)&addr_ip4;

	int r;
	r = uv_ip4_addr("0.0.0.0", port, &addr_ip4);

	r = uv_tcp_bind(&server, addr, 0);

	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "bind: %s\n", uv_strerror(err));
		return -1;
	}

	r = uv_listen((uv_stream_t *)&server, 128, on_connected);

	if(r)
	{
		uv_errno_t err = r;
		fprintf(stderr, "listen %s\n", uv_strerror(err));
		return -1;
	}

	return 0;
}

int
chimaerad_http_deinit()
{
	uv_close((uv_handle_t *)&server, NULL);
	
	return 0;
}
