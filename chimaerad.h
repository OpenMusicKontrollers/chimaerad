#ifndef _CHIMAERAD_H
#define _CHIMAERAD_H

// include libuv
#include <uv.h>

int chimaerad_http_init(uv_loop_t *loop, uint16_t port);
int chimaerad_http_deinit();

#endif // _CHIMAERAD_H
