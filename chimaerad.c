#include <stdio.h>

#include <chimaerad.h>

int
main(int argc, char **argv)
{
	uv_loop_t *loop = uv_default_loop();

	int r;
	if( (r = chimaerad_http_init(loop, 9000)) )
		return r;

	uv_run(loop, UV_RUN_DEFAULT);

	if( (r = chimaerad_http_deinit()) )
		return r;

	return 0;
}
