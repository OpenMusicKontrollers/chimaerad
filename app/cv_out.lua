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
	local idx = gid + 1
	local io = self.io[idx]

	io.xpos(time, x)
	io.zpos(time, z)
	if(X and Z) then
		io.xvel(time, X)
		io.zvel(time, Z)
	end

	io.gate(time, 1)
	self.idxs[sid] = idx
end

local function off(self, time, sid)
	local idx = self.idxs[sid]
	local io = self.io[idx]

	io.gate(time, 0)
	self.idxs[sid] = nil
end

local function set(self, time, sid, x, z, X, Z)
	local idx = self.idxs[sid]
	local io = self.io[idx]

	io.xpos(time, x)
	io.zpos(time, z)
	if(X and Z) then
		io.xvel(time, X)
		io.zvel(time, Z)
	end
end

local midi_out = osc_responder:new({
	_init = function(self)
		self.idxs = {}

		self.io = {}
		for i = 1, 2 do
			self.io[i] = {
				gate = JACK_CV.new({port='cv_gate_' .. i}),
				xpos = JACK_CV.new({port='cv_xpos_' .. i}),
				zpos = JACK_CV.new({port='cv_zpos_' .. i}),
				xvel = JACK_CV.new({port='cv_xvel_' .. i}),
				zvel = JACK_CV.new({port='cv_zvel_' .. i})
			}
		end
	end,

	_deinit = function(self)
		for i, v in ipairs(self.io) do
			for k, w in pairs(v) do
				w:close()
			end
		end
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

return midi_out
