--[[
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
--]]

local osc_responder = require('osc_responder')

local function on(self, time, sid, gid, pid, x, z, X, Z)
	sid = sid%self.wrap + self.sid_offset

	self.io(time, '/s_new', 'siiiiisisi',
		self.inst[gid+1], sid, 0, gid+self.gid_offset,
		4, pid,
		'out', gid+self.out_offset,
		'gate', 1)

	if(X and Z) then
		self.io(time, '/n_setn', 'iiiffff',
			sid, 0, 4, x, z, X, Z)
	else
		self.io(time, '/n_setn', 'iiiff',
			sid, 0, 2, x, z)
	end
end

local function off(self, time, sid)
	sid = sid%self.wrap + self.sid_offset

	self.io(time, '/n_set', 'isi',
		sid, 'gate', 0)
end

local function set(self, time, sid, x, z, X, Z)
	sid = sid%self.wrap + self.sid_offset

	if(X and Z) then
		self.io(time, '/n_setn', 'iiiffff',
			sid, 0, 4, x, z, X, Z)
	else
		self.io(time, '/n_setn', 'iiiff',
			sid, 0, 2, x, z)
	end
end

local osc_out = osc_responder:new({
	out_offset = 0,
	gid_offset = 100,
	sid_offset = 200,
	wrap = 100,
	inst = {'inst1', 'inst2', 'inst3', 'inst4', 'inst5', 'inst6', 'inst7', 'inst8'},

	_init = function(self)
		self.io = JACK_OSC.new({port='scsynth_out_1'})
	end,

	_deinit = function(self)
		self.io:close()
	end,

	close = function(self)
		self:_deinit()
	end,

	_root =  {
		on = on,
		off = off,
		set = set,
		idle = nil
	}
})

return osc_out
