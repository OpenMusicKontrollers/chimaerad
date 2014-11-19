/*
 * Copyright (c) 2014 Hanspeter Portner (dev@open-music-kontrollers.ch)
 * 
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 * 
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 * 
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 * 
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 * 
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 */

#include <stdio.h>

#include <osc_stream_private.h>

static void
_udp_alloc(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	uv_udp_t *socket = (uv_udp_t *)handle;
	osc_stream_udp_t *udp = (void *)socket - offsetof(osc_stream_udp_t, socket);
	osc_stream_t *stream = (void *)udp - offsetof(osc_stream_t, payload.udp);
	osc_stream_cb_t *cb = &stream->cb;

	buf->base = cb->buf;
	buf->len = OSC_STREAM_BUF_SIZE;
}

static void
_udp_recv_cb(uv_udp_t *handle, ssize_t nread, const uv_buf_t *buf, const struct sockaddr *addr, unsigned int flags)
{
	uv_udp_t *socket = (uv_udp_t *)handle;
	osc_stream_udp_t *udp = (void *)socket - offsetof(osc_stream_udp_t, socket);
	osc_stream_t *stream = (void *)udp - offsetof(osc_stream_t, payload.udp);
	osc_stream_cb_t *cb = &stream->cb;

	if(nread > 0)
	{
		if(osc_check_packet((osc_data_t *)buf->base, buf->len))
			fprintf(stderr, "_udp_recv_cb: wrongly formatted OSC packet\n");
		else
			if(cb->recv)
				cb->recv(stream, (osc_data_t *)buf->base, nread, cb->data);
	}
	else if (nread < 0)
	{
		uv_close((uv_handle_t *)handle, NULL);
		fprintf(stderr, "_udp_recv_cb_cb: %s\n", uv_err_name(nread));
	}
}

void
getaddrinfo_udp_tx_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *res)
{
	uv_loop_t *loop = req->loop;
	osc_stream_udp_t *udp = (void *)req - offsetof(osc_stream_udp_t, req);
	osc_stream_udp_tx_t *tx = &udp->tx;
	osc_stream_t *stream = (void *)udp - offsetof(osc_stream_t, payload.udp);
	osc_stream_cb_t *cb = &stream->cb;

	osc_stream_addr_t src;
	char remote [128] = {'\0'};
	unsigned int flags = 0;

	int err;
	switch(udp->version)
	{
		case OSC_STREAM_IP_VERSION_4:
		{
			struct sockaddr_in *ptr4 = (struct sockaddr_in *)res->ai_addr;
			if((err = uv_ip4_name(ptr4, remote, 127)))
			{
				fprintf(stderr, "up_ip4_name: %s\n", uv_err_name(err));
				return;
			}
			if((err = uv_udp_init(loop, &udp->socket)))
			{
				fprintf(stderr, "uv_udp_init: %s\n", uv_err_name(err));
				return;
			}
			if((err = uv_ip4_addr(remote, ntohs(ptr4->sin_port), &tx->addr.ip4)))
			{
				fprintf(stderr, "uv_ip4_addr: %s\n", uv_err_name(err));
				return;
			}

			if((err = uv_ip4_addr("0.0.0.0", 0, &src.ip4)))
			{
				fprintf(stderr, "up_ip4_addr: %s\n", uv_err_name(err));
				return;
			}

			break;
		}
		case OSC_STREAM_IP_VERSION_6:
		{
			struct sockaddr_in6 *ptr6 = (struct sockaddr_in6 *)res->ai_addr;
			if((err = uv_ip6_name(ptr6, remote, 127)))
			{
				fprintf(stderr, "up_ip6_name: %s\n", uv_err_name(err));
				return;
			}
			if((err = uv_udp_init(loop, &udp->socket)))
			{
				fprintf(stderr, "uv_udp_init: %s\n", uv_err_name(err));
				return;
			}
			if((err = uv_ip6_addr(remote, ntohs(ptr6->sin6_port), &tx->addr.ip6)))
			{
				fprintf(stderr, "uv_ip6_addr: %s\n", uv_err_name(err));
				return;
			}
			
			if((err = uv_ip6_addr("::", 0, &src.ip6)))
			{
				fprintf(stderr, "up_ip6_addr: %s\n", uv_err_name(err));
				return;
			}
			flags |= UV_UDP_IPV6ONLY; //TODO make this configurable?

			break;
		}
	}
	
	if((err = uv_udp_bind(&udp->socket, &src.ip, flags)))
	{
		fprintf(stderr, "uv_udp_bind: %s\n", uv_err_name(err));
		return;
	}
	if((err = uv_udp_recv_start(&udp->socket, _udp_alloc, _udp_recv_cb)))
	{
		fprintf(stderr, "uv_udp_recv_start: %s\n", uv_err_name(err));
		return;
	}

	uv_freeaddrinfo(res);

	if(cb->recv)
		cb->recv(stream, (osc_data_t *)resolve_msg, sizeof(resolve_msg), cb->data);
}

void
getaddrinfo_udp_rx_cb(uv_getaddrinfo_t *req, int status, struct addrinfo *res)
{
	uv_loop_t *loop = req->loop;
	osc_stream_udp_t *udp = (void *)req - offsetof(osc_stream_udp_t, req);
	osc_stream_t *stream = (void *)udp - offsetof(osc_stream_t, payload.udp);
	osc_stream_cb_t *cb = &stream->cb;

	osc_stream_addr_t src;
	unsigned int flags = 0;

	int err;
	if((err = uv_udp_init(loop, &udp->socket)))
	{
		fprintf(stderr, "uv_udp_init: %s\n", uv_err_name(err));
		return;
	}

	switch(udp->version)
	{
		case OSC_STREAM_IP_VERSION_4:
		{
			struct sockaddr_in *ptr4 = (struct sockaddr_in *)res->ai_addr;
			if((err = uv_ip4_addr("0.0.0.0", ntohs(ptr4->sin_port), &src.ip4)))
			{
				fprintf(stderr, "uv_ip4_addr: %s\n", uv_err_name(err));
				return;
			}

			break;
		}
		case OSC_STREAM_IP_VERSION_6:
		{
			struct sockaddr_in6 *ptr6 = (struct sockaddr_in6 *)res->ai_addr;
			if((err = uv_ip6_addr("::", ntohs(ptr6->sin6_port), &src.ip6)))
			{
				fprintf(stderr, "uv_ip6_addr: %s\n", uv_err_name(err));
				return;
			}
			flags |= UV_UDP_IPV6ONLY; //TODO make this configurable?

			break;
		}
	}
	
	if((err = uv_udp_bind(&udp->socket, &src.ip, flags)))
	{
		fprintf(stderr, "uv_udp_bind: %s\n", uv_err_name(err));
		return;
	}
	if((err = uv_udp_recv_start(&udp->socket, _udp_alloc, _udp_recv_cb)))
	{
		fprintf(stderr, "uv_udp_recv_start: %s\n", uv_err_name(err));
		return;
	}

	uv_freeaddrinfo(res);

	if(cb->recv)
		cb->recv(stream, (osc_data_t *)resolve_msg, sizeof(resolve_msg), cb->data);
}

static void
_udp_send_cb(uv_udp_send_t *req, int status)
{
	osc_stream_udp_tx_t *tx = (void *)req - offsetof(osc_stream_udp_tx_t, req);
	osc_stream_udp_t *udp = (void *)tx - offsetof(osc_stream_udp_t, tx);
	osc_stream_t *stream = (void *)udp - offsetof(osc_stream_t, payload.udp);
	osc_stream_cb_t *cb = &stream->cb;

	if(!status)
	{
		if(cb->send)
			cb->send(stream, tx->len, cb->data);
	}
	else
		fprintf(stderr, "_udp_send_cb: %s\n", uv_err_name(status));
}

void
osc_stream_udp_send(osc_stream_t *stream, const osc_data_t *buf, size_t len)
{
	osc_stream_udp_t *udp = &stream->payload.udp;
	udp->tx.len = len;
	
	uv_buf_t msg [1] = {
		[0] = {
			.base = (char *)buf,
			.len = len
		}
	};

	int err;
	if((err = uv_udp_send(&udp->tx.req, &udp->socket, &msg[0], 1, &udp->tx.addr.ip, _udp_send_cb)))
		fprintf(stderr, "uv_udp_send: %s", uv_err_name(err));
}

void
osc_stream_udp_send2(osc_stream_t *stream, const uv_buf_t *bufs, size_t bufn)
{
	osc_stream_udp_t *udp = &stream->payload.udp;
	udp->tx.len = 0;

	int i;
	for(i=0; i<bufn; i++)
		udp->tx.len += bufs[i].len;

	int err;
	if((err = uv_udp_send(&udp->tx.req, &udp->socket, bufs, bufn, &udp->tx.addr.ip, _udp_send_cb)))
		fprintf(stderr, "uv_udp_send: %s", uv_err_name(err));
}
