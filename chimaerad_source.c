#include <chimaerad.h>

#include <sys/mman.h>

#define AREA_SIZE 0x1000000UL // 16MB

static inline void *
_rt_alloc(chimaerad_source_t *source, size_t len)
{
	void *data = NULL;
	
	if(!(data = tlsf_malloc(source->tlsf, len)))
		; //FIXME

	return data;
}

static inline void *
_rt_realloc(chimaerad_source_t *source, size_t len, void *buf)
{
	void *data = NULL;
	
	if(!(data =tlsf_realloc(source->tlsf, buf, len)))
		; //FIXME

	return data;
}

static inline void
_rt_free(chimaerad_source_t *source, void *buf)
{
	tlsf_free(source->tlsf, buf);
}

static void *
_lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
	chimaerad_source_t *source = ud;
	(void)osize;

	if(nsize == 0) {
		if(ptr)
			_rt_free(source, ptr);
		return NULL;
	}
	else {
		if(ptr)
			return _rt_realloc(source, nsize, ptr);
		else
			return _rt_alloc(source, nsize);
	}
}

static void
_quit(uv_async_t *handle)
{
	chimaerad_source_t *source = handle->data;

	osc_stream_deinit(&source->data);
	uv_close((uv_handle_t *)&source->quit, NULL); // is actually THIS handle
}

static void
_thread(void *arg)
{
	chimaerad_source_t *source = arg;

	struct sched_param schedp = {
		.sched_priority = 50
	};
	pthread_setschedparam(source->thread, SCHED_RR, &schedp);

	uv_run(&source->loop, UV_RUN_DEFAULT);
}

static int
_on_osc_data_stream_resolve(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;

	//TODO
	return 1;
}

static int
_on_osc_data_on(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;
	lua_State *L = source->L;
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
_on_osc_data_off(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;
	lua_State *L = source->L;
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
_on_osc_data_set(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;
	lua_State *L = source->L;
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
_on_osc_data_idle(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;
	lua_State *L = source->L;

	lua_getglobal(L, "idle");
	if(lua_isfunction(L, -1))
	{
		lua_pushnumber(L, time);
		if(lua_pcall(L, 1, 0, 0))
		{
			fprintf(stderr, "err: %s\n", lua_tostring(L, -1));
			return -1;
		}
	}
	else
		lua_pop(L, 1); // "idle"

	return 1;
}

static int
_on_osc_data_frm(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	osc_data_t *ptr = arg;
	uint32_t fid;
	uint64_t tt;
	
	ptr = osc_get_int32(ptr, (int32_t *)&fid);
	ptr = osc_get_timetag(ptr, &tt);

	printf("frm: %u %16lx\n", fid, tt);

	//TODO
	return 1;
}

static int
_on_osc_data_tok(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	osc_data_t *ptr = arg;
	uint32_t sid;
	uint32_t pid;
	uint32_t gid;
	float x;
	float z;
	//float a;

	ptr = osc_get_int32(ptr, (int32_t *)&sid);
	ptr = osc_get_int32(ptr, (int32_t *)&pid);
	ptr = osc_get_int32(ptr, (int32_t *)&gid);
	ptr = osc_get_float(ptr, &x);
	ptr = osc_get_float(ptr, &z);
	//ptr = osc_get_float(ptr, &a);

	printf("tok: %u %u %u %f %f\n", sid, gid, pid, x, z);

	//TODO
	return 1;
}

static int
_on_osc_data_alv(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	osc_data_t *ptr = arg;
	uint32_t sid;

	printf("alv:");
	for(int i=0; i<strlen(fmt); i++)
	{
		ptr = osc_get_int32(ptr, (int32_t *)&sid);
		printf(" %u", sid);
	}
	printf("\n");

	//TODO
	return 1;
}

static const osc_method_t on_osc_data [] = {
	{"/stream/resolve", "", _on_osc_data_stream_resolve},

	{"/on", "iiiff", _on_osc_data_on},
	{"/off", "i", _on_osc_data_off},
	{"/set", "iff", _on_osc_data_set},
	{"/idle", "", _on_osc_data_idle},

	{"/tuio2/frm", "it", _on_osc_data_frm},
	{"/tuio2/tok", "iiifff", _on_osc_data_tok},
	{"/tuio2/alv", NULL, _on_osc_data_alv},

	{NULL, NULL, NULL}
};

static void
_on_osc_data_recv(osc_stream_t *stream, osc_data_t *buf, size_t len , void *data)
{
	chimaerad_source_t *source = data;

	//printf("_on_osc_data_recv: %zu %s\n", len, buf);
	
	if(osc_check_packet(buf, len))
		osc_dispatch_method(0, buf, len, (osc_method_t *)on_osc_data, NULL, NULL, source); //TODO use timestamp
}

static int
_on_osc_config_stream_resolve(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;

	static osc_data_t buf [512];
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + 512;

	char dest [32];
	sprintf(dest, "%u.%u.%u.%u:3333",
		(source->iface->ip4 >> 24) & 0xff,
		(source->iface->ip4 >> 16) & 0xff,
		(source->iface->ip4 >> 8) & 0xff,
		(source->iface->ip4 >> 0) & 0xff);

	ptr = osc_set_vararg(ptr, end, "/engines/address", "is", 13, dest);
	osc_stream_send(&source->config, buf, ptr-buf);

	return 1;
}

static int
_on_osc_config_success(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;

	osc_data_t *ptr = arg;
	int32_t uuid;
	const char *query;

	ptr = osc_get_int32(ptr, &uuid);
	ptr = osc_get_string(ptr, &query);
	
	fprintf(stderr, "success on: %i '%s'\n", uuid, query);

	if(!strcmp(query, "/engines/address"))
	{
		static osc_data_t buf [512];
		osc_data_t *ptr = buf;
		osc_data_t *end = buf + 512;

		osc_data_t *bndl;
		ptr = osc_start_bundle(ptr, end, OSC_IMMEDIATE, &bndl);
		{
			osc_data_t *itm;
			ptr = osc_set_bundle_item(ptr, end, "/sensors/rate", "ii", 10, source->rate);
			ptr = osc_set_bundle_item(ptr, end, "/engines/parallel", "ii", 11, source->rate >= 2000 ? 1 : 0);

			ptr = osc_set_bundle_item(ptr, end, "/engines/enabled", "ii", 12, 0);
			ptr = osc_set_bundle_item(ptr, end, "/engines/reset", "i", 13);
		
			if(source->mode == CHIMAERAD_SOURCE_MODE_UDP)
			{
				ptr = osc_set_bundle_item(ptr, end, "/engines/tuio2/enabled", "ii", 14, 1);
				ptr = osc_set_bundle_item(ptr, end, "/engines/mode", "is", 15, "osc.udp");
			}
			else // CHIMAERAD_SOURCE_MODE_TCP
			{
				ptr = osc_set_bundle_item(ptr, end, "/engines/dummy/enabled", "ii", 14, 1);
				ptr = osc_set_bundle_item(ptr, end, "/engines/dummy/redundancy", "ii", 15, 0);
				ptr = osc_set_bundle_item(ptr, end, "/engines/mode", "is", 16, "osc.tcp");
			}

			ptr = osc_set_bundle_item(ptr, end, "/engines/server", "ii", 17, 0);
			ptr = osc_set_bundle_item(ptr, end, "/engines/enabled", "ii", 18, 1);
		}
		ptr = osc_end_bundle(ptr, end, bndl);

		osc_stream_send(&source->config, buf, ptr - buf);
	}

	return 1;
}

static int
_on_osc_config_fail(osc_time_t time, const char *path, const char *fmt, osc_data_t *arg, size_t size, void *data)
{
	chimaerad_source_t *source = data;
	
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

static const osc_method_t on_osc_config [] = {
	{"/stream/resolve", "", _on_osc_config_stream_resolve},
	{"/success", NULL, _on_osc_config_success},
	{"/fail", "iss", _on_osc_config_fail},
	{NULL, NULL, NULL}
};

static void
_on_osc_config_recv(osc_stream_t *stream, osc_data_t *buf, size_t len , void *data)
{
	chimaerad_source_t *source = data;

	//printf("_on_osc_config_recv: %zu %s\n", len, buf);
	//TODO

	if(osc_check_packet(buf, len))
		osc_dispatch_method(0, buf, len, (osc_method_t *)on_osc_config, NULL, NULL, source);
}

static void
_on_osc_config_send(osc_stream_t *stream, size_t len , void *data)
{
	chimaerad_source_t *source = data;

	//printf("_on_osc_config_send: %zu\n", len);
	//TODO
}

	/*
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
*/

static int
_midi(lua_State *L)
{
	chimaerad_host_t *host = lua_touserdata(L, lua_upvalueindex(1));

	uint8_t msg [3] = {0, 0, 0}; //TODO how big?

	int n = lua_gettop(L);
	for(int i=0; i<n; i++)
		msg[i] = luaL_checkinteger(L, i+1);

	rtmidic_out_send_message(host->midi, 3, msg);

	return 0;
}

int
chimaerad_source_init(uv_loop_t *loop, chimaerad_source_t *source)
{
#if defined(__WINDOWS__)
	source->area = malloc(AREA_SIZE);
#elif defined(__linux__) || defined(__CYGWIN__)
	source->area = mmap(NULL, AREA_SIZE, PROT_READ|PROT_WRITE, MAP_32BIT|MAP_PRIVATE|MAP_ANONYMOUS|MAP_LOCKED, -1, 0);
#elif defined(__APPLE__)
	source->area = mmap(NULL, AREA_SIZE, PROT_READ|PROT_WRITE, MAP_PRIVATE, -1, 0);
#endif
	source->tlsf = tlsf_create_with_pool(source->area, AREA_SIZE);
	source->pool = tlsf_get_pool(source->tlsf);
	
	source->L = lua_newstate(_lua_alloc, source);
	luaL_openlibs(source->L);

	// overwrite loader functions with our own
	/*
	lua_getglobal(source->L, "package");
	{
		lua_newtable(source->L);
		{
			lua_pushlightuserdata(source->L, source->host);
			lua_pushcclosure(source->L, _eet_loader, 1);
			lua_rawseti(source->L, -2, 1);
		}
		lua_setfield(source->L, -2, "loaders");
	}
	lua_pop(source->L, 1); // package
	*/

	lua_pushlightuserdata(source->L, source->host);
	lua_pushcclosure(source->L, _midi, 1);
	lua_setglobal(source->L, "midi");

	if(luaL_dostring(source->L, "require('dummy')"))
	{
		fprintf(stderr, "err: %s\n", lua_tostring(source->L, -1));
		return -1;
	}
	
	uv_loop_init(&source->loop);
	source->quit.data = source;
	uv_async_init(&source->loop, &source->quit, _quit);
	uv_thread_create(&source->thread, _thread, source);

	if(source->mode == CHIMAERAD_SOURCE_MODE_UDP)
		osc_stream_init(&source->loop, &source->data, "osc.udp4://:3333", _on_osc_data_recv, NULL, source);
	else // CHIMAERAD_SOURCE_MODE_TCP
		osc_stream_init(&source->loop, &source->data, "osc.tcp4://:3333", _on_osc_data_recv, NULL, source);
	char dest [128];
	sprintf(dest, "osc.udp4://%u.%u.%u.%u:4444",
		(source->ip4 >> 24) & 0xff,
		(source->ip4 >> 16) & 0xff,
		(source->ip4 >> 8) & 0xff,
		(source->ip4 >> 0) & 0xff);
	osc_stream_init(loop, &source->config, dest, _on_osc_config_recv, _on_osc_config_send, source);

	source->claimed = 1;

	return 0;
}

int
chimaerad_source_deinit(chimaerad_source_t *source)
{
	if(!source->claimed)
		return 0;

	static osc_data_t buf [512];
	osc_data_t *ptr = buf;
	osc_data_t *end = buf + 512;

	osc_data_t *bndl;
	ptr = osc_start_bundle(ptr, end, OSC_IMMEDIATE, &bndl);
	{
		ptr = osc_set_bundle_item(ptr, end, "/engines/enabled", "ii", 12, 0);
		ptr = osc_set_bundle_item(ptr, end, "/engines/reset", "i", 13);
	}
	osc_stream_send(&source->config, buf, ptr - buf);

	osc_stream_deinit(&source->config);

	uv_async_send(&source->quit);
	uv_thread_join(&source->thread);

	//osc_stream_deinit(&source->data);
	//uv_close((uv_handle_t *)&source->quit, NULL);
	uv_loop_close(&source->loop);

	lua_close(source->L);

	tlsf_remove_pool(source->tlsf, source->pool);
	tlsf_destroy(source->tlsf);
#if defined(__WINDOWS__)
	free(source->area);
	source->area = malloc(AREA_SIZE);
#elif defined(__linux__) || defined(__CYGWIN__)
	munmap(source->area, AREA_SIZE);
#elif defined(__APPLE__)
	munmap(source->area, AREA_SIZE);
#endif

	source->claimed = 0;

	return 0;
}
