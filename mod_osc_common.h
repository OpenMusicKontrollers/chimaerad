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

#ifndef _CHIMAERAD_MOD_OSC_COMMON_H
#define _CHIMAERAD_MOD_OSC_COMMON_H

#include <stdint.h>

#include <osc.h>

typedef struct _mod_blob_t mod_blob_t;

struct _mod_blob_t {
	int32_t size;
	uint8_t buf [0];
};

osc_data_t *mod_osc_encode(lua_State *L, int pos, osc_data_t *buf, osc_data_t
*end);

#endif
