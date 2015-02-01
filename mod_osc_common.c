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

#include <lua.h>
#include <lauxlib.h>

#include <osc.h>
#include <mod_osc_common.h>

osc_data_t *
mod_osc_encode(lua_State *L, int pos, osc_data_t *buf, osc_data_t *end)
{
	osc_data_t *ptr = buf;
	
	const char *path = luaL_checkstring(L, pos++);
	const char *fmt = luaL_checkstring(L, pos++);

	ptr = osc_set_path(ptr, end, path);
	ptr = osc_set_fmt(ptr, end, fmt);

	for(const char *type = fmt; *type; type++)
		switch(*type)
		{
			case OSC_INT32:
			{
				int32_t i = luaL_checkint(L, pos++);
				ptr = osc_set_int32(ptr, end, i);
				break;
			}
			case OSC_FLOAT:
			{
				float f = luaL_checknumber(L, pos++);
				ptr = osc_set_float(ptr, end, f);
				break;
			}
			case OSC_STRING:
			{
				const char *s = luaL_checkstring(L, pos++);
				ptr = osc_set_string(ptr, end, s);
				break;
			}
			case OSC_BLOB:
			{
				mod_blob_t *tb = luaL_checkudata(L, pos++, "mod_blob_t");
				ptr = osc_set_blob(ptr, end, tb->size, tb->buf);
				break;
			}
			
			case OSC_TIMETAG:
			{
				osc_time_t t = luaL_checknumber(L, pos++);
				ptr = osc_set_timetag(ptr, end, t);
				break;
			}
			case OSC_INT64:
			{
				int64_t h = luaL_checknumber(L, pos++);
				ptr = osc_set_int64(ptr, end, h);
				break;
			}
			case OSC_DOUBLE:
			{
				double d = luaL_checknumber(L, pos++);
				ptr = osc_set_double(ptr, end, d);
				break;
			}
			
			case OSC_NIL:
			case OSC_BANG:
			case OSC_TRUE:
			case OSC_FALSE:
				break;
			
			case OSC_SYMBOL:
			{
				const char *S = luaL_checkstring(L, pos++);
				ptr = osc_set_symbol(ptr, end, S);
				break;
			}
			case OSC_CHAR:
			{
				char c = luaL_checkint(L, pos++);
				ptr = osc_set_char(ptr, end, c);
				break;
			}
			case OSC_MIDI:
			{
				uint8_t *m;
				ptr = osc_set_midi_inline(ptr, end, &m);
				if(lua_istable(L, pos))
				{
					lua_rawgeti(L, pos, 4);
					lua_rawgeti(L, pos, 3);
					lua_rawgeti(L, pos, 2);
					lua_rawgeti(L, pos, 1);
					m[0] = luaL_checkint(L, -1);
					m[1] = luaL_checkint(L, -2);
					m[2] = luaL_checkint(L, -3);
					m[3] = luaL_checkint(L, -4);
					lua_pop(L, 4);
				}
				else
				{
					memset(m, 0, 4);
				}
				pos++;
				break;
			}
		}

	return ptr;
}
