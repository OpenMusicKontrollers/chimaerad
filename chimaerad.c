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

#include <stdio.h>

#include <chimaerad.h>

int
main(int argc, char **argv)
{
	/*
	int r;
	uv_loop_t *loop;
	static chimaerad_host_t host;

	loop = uv_default_loop();

	if( (r = chimaerad_host_init(loop, &host, 9000)) )
		return r;

	uv_run(loop, UV_RUN_DEFAULT);

	if( (r = chimaerad_host_deinit(&host)) )
		return r;
	*/

	uv_loop_t *loop = uv_default_loop();
	lua_State *L = luaL_newstate();

	luaL_openlibs(L);
	luaopen_json(L);
	luaopen_osc(L);
	luaopen_http(L);
	luaopen_zip(L);

	if(luaL_dofile(L, argv[1]))
		fprintf(stderr, "main: %s\n", lua_tostring(L, -1));

	uv_run(loop, UV_RUN_DEFAULT);

	lua_close(L);

	return 0;
}
