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
local bit32 = bit32 or bit

local function on(self, time, sid, gid, pid, x, z, X, Z)
	local key = self.map(x)
	local base = math.floor(key)
	local bend = (key-base)/self.map.range*0x2000 + 0x1fff
	local eff = z*0x3fff
	local eff_msb = bit32.rshift(eff, 7)
	local eff_lsb = bit32.band(eff, 0x7f)

	self.midi(time, -- note on
		bit32.bor(0x90, gid),
		base,
		0x7f)
	self.midi(time, -- pitch bend
		bit32.bor(0xe0, gid),
		bit32.band(bend, 0x7f),
		bit32.rshift(bend, 7))
	self.midi(time, -- note pressure
		bit32.bor(0xa0, gid),
		base,
		eff_msb)
	if(self.control <= 0xd) then
		self.midi(time, -- control change
			bit32.bor(0xb0, gid),
			bit32.bor(0x20, self.control),
			eff_lsb)
	end
	self.midi(time, -- control change
		bit32.bor(0xb0, gid),
		self.control,
		eff_msb)
	
	self.gids[sid] = gid
	self.bases[sid] = base
end

local function off(self, time, sid)
	local gid = self.gids[sid]
	local base = self.bases[sid]
	
	self.midi(time, -- note off
		bit32.bor(0x80, gid),
		base,
		0x7f)

	self.gids[sid] = nil
	self.bases[sid] = nil
end

local function set(self, time, sid, x, z, X, Z)
	local gid = self.gids[sid]
	local key = self.map(x)
	local base = self.bases[sid]
	local bend = (key-base)/self.map.range*0x2000 + 0x1fff
	local eff = z*0x3fff
	local eff_msb = bit32.rshift(eff, 7)
	local eff_lsb = bit32.band(eff, 0x7f)

	self.midi(time, -- pitch bend
		bit32.bor(0xe0, gid),
		bit32.band(bend, 0x7f),
		bit32.rshift(bend, 7))
	self.midi(time, -- note pressure
		bit32.bor(0xa0, gid),
		base,
		eff_msb)
	if(self.control <= 0xd) then
		self.midi(time, -- control change
			bit32.bor(0xb0, gid),
			bit32.bor(0x20, self.control),
			eff_lsb)
	end
	self.midi(time, -- control change
		bit32.bor(0xb0, gid),
		self.control,
		eff_msb)
end

local midi_out = osc_responder:new({
	map = map_linear,
	control = 0x07, -- volume

	_init = function(self)
		self.gids = {}
		self.bases = {}
		--self.midi = RTMIDI.new('UNIX_JACK')
		--self.midi:open_virtual()
		self.midi = JACK_MIDI.new({port='midi_out_1'})
	end,

	_deinit = function(self)
		self.midi:close()
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
