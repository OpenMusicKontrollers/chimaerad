#include <stdio.h>

#include <chimaerad.h>

int
main(int argc, char **argv)
{
	int r;
	uv_loop_t *loop;
	static chimaerad_host_t host;

	eina_init();
	eet_init();
	
	loop = uv_default_loop();

	if( (r = chimaerad_host_init(loop, &host, 9000)) )
		return r;

	uv_run(loop, UV_RUN_DEFAULT);

	if( (r = chimaerad_host_deinit(&host)) )
		return r;

	eet_shutdown();
	eina_shutdown();

	return 0;
}
