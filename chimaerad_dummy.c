#include <chimaerad.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// include osc_stream
#include <osc.h>
#include <osc_stream.h>

typedef struct _dummy_t dummy_t;

struct _dummy_t {
	lua_State *L;
	osc_stream_t stream;
};

static dummy_t dummy;

static int 
_on(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *dat)
{
	dummy_t *dummy = dat;
	lua_State *L = dummy->L;

	osc_data_t *ptr = arg;
	int32_t sid;
	int32_t gid;
	int32_t pid;
	float x;
	float z;

	ptr = osc_get_int32(ptr, &sid);
	ptr = osc_get_int32(ptr, &gid);
	ptr = osc_get_int32(ptr, &pid);
	ptr = osc_get_float(ptr, &x);
	ptr = osc_get_float(ptr, &z);

	lua_getglobal(L, "on");
	if(lua_isfunction(L, -1))
	{
		lua_pushnumber(L, time);
		lua_pushnumber(L, sid);
		lua_pushnumber(L, gid);
		lua_pushnumber(L, pid);
		lua_pushnumber(L, x);
		lua_pushnumber(L, z);

		if(lua_pcall(L, 6, 0, 0))
		{
			fprintf(stderr, "err: %s\n", lua_tostring(L, -1));
			return -1;
		}
	}
	else
		lua_pop(L, 1); // "on"

	return 1;
}

static int 
_off(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *dat)
{
	dummy_t *dummy = dat;
	lua_State *L = dummy->L;

	osc_data_t *ptr = arg;
	int32_t sid;

	ptr = osc_get_int32(ptr, &sid);

	lua_getglobal(L, "off");
	if(lua_isfunction(L, -1))
	{
		lua_pushnumber(L, time);
		lua_pushnumber(L, sid);

		if(lua_pcall(L, 2, 0, 0))
		{
			fprintf(stderr, "err: %s\n", lua_tostring(L, -1));
			return -1;
		}
	}
	else
		lua_pop(L, 1); // "off"

	return 1;
}

static int 
_set(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *dat)
{
	dummy_t *dummy = dat;
	lua_State *L = dummy->L;

	osc_data_t *ptr = arg;
	int32_t sid;
	float x;
	float z;

	ptr = osc_get_int32(ptr, &sid);
	ptr = osc_get_float(ptr, &x);
	ptr = osc_get_float(ptr, &z);

	lua_getglobal(L, "set");
	if(lua_isfunction(L, -1))
	{
		lua_pushnumber(L, time);
		lua_pushnumber(L, sid);
		lua_pushnumber(L, x);
		lua_pushnumber(L, z);

		if(lua_pcall(L, 4, 0, 0))
		{
			fprintf(stderr, "err: %s\n", lua_tostring(L, -1));
			return -1;
		}
	}
	else
		lua_pop(L, 1); // "set"

	return 1;
}

static int 
_idle(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *dat)
{
	dummy_t *dummy = dat;
	lua_State *L = dummy->L;

	lua_getglobal(L, "idle");
	if(lua_isfunction(L, -1))
	{
		if(lua_pcall(L, 0, 0, 0))
		{
			fprintf(stderr, "err: %s\n", lua_tostring(L, -1));
			return -1;
		}
	}
	else
		lua_pop(L, 1); // "idle"

	return 1;
}

static const osc_method_t methods [] = {
	{"/on", "iiiff", _on},
	{"/off", "i", _off},
	{"/set", "iff", _set},
	{"/idle", "", _idle},
	{NULL, NULL, NULL},
};

static void
_recv_cb(osc_stream_t *stream, osc_data_t *buf, size_t len , void *data)
{
	osc_dispatch_method(0, buf, len, (osc_method_t *)methods, NULL, NULL, data);
}

static void
_send_cb(osc_stream_t *stream, size_t len , void *data)
{

}

int
chimaerad_dummy_init(uv_loop_t *loop, uint16_t port)
{
	if(!dummy.L)
	{
		dummy.L = lua_open();
		luaL_openlibs(dummy.L);
		if(luaL_dofile(dummy.L, "chimaerad_dummy.lua"))
			fprintf(stderr, "err: %s\n", lua_tostring(dummy.L, -1));
	}

	if(osc_stream_init(loop, &dummy.stream, "osc.udp4://:3333", _recv_cb, _send_cb, &dummy))
		return -1;

	return 0;
}

int osc_stream_init(uv_loop_t *loop, osc_stream_t *stream, const char *addr, osc_stream_recv_cb_t recv_cb, osc_stream_send_cb_t send_cb, void *data);
//void osc_stream_send(osc_stream_t *stream, const osc_data_t *buf, size_t len);
//void osc_stream_send2(osc_stream_t *stream, const uv_buf_t *bufs, size_t bufn);

int
chimaerad_dummy_deinit()
{
	osc_stream_deinit(&dummy.stream);

	if(dummy.L)
		lua_close(dummy.L);
	
	return 0;
}
