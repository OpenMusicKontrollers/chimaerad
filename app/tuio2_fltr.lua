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

local function tuio2_frm(self, time, fid, stamp, dim, src)
	if ( (fid > self.last_fid) and (time >= self.last_time) or (fid == 1) ) then
		self.ignore = false
		self.last_fid = fid
		self.last_time = time
	else
		print('tuio2_fltr: ignoring out-of-order frame #'..fid)
		self.ignore = true
		return
	end

	self.old_blobs = self.new_blobs or {}
end

local function tuio2_tok(self, time, sid, pid, gid, x, y, a, X, Y, A, m, R)
	if self.ignore then return end

	-- add blob to hash
	local elmnt
	elmnt = self.blobs[sid] or {0, 0, 0, 0, 0, 0, 0}
	elmnt[1] = sid
	elmnt[2] = gid
	elmnt[3] = pid
	elmnt[4] = x
	elmnt[5] = y
	-- a is ignored
	elmnt[6] = X -- may be nil
	elmnt[7] = Y -- may be nil
	self.blobs[sid] = elmnt
end

local function tuio2_alv(self, time, ...)
	if self.ignore then return end

	local v, w
	self.new_blobs = {...}

	-- are there any blobs active?
	if #self.new_blobs == 0 and #self.old_blobs == 0 then
		self.cb(time, '/idle', '')
		return
	end

	-- have any blobs disappeared?
	for _, v in ipairs(self.old_blobs) do
		local found = false
		for _, w in ipairs(self.new_blobs) do
			if v == w then
				found = true
				break
			end
		end
		if not found then
			local b = self.blobs[v]
			self.cb(time, '/off', 'i', b[1])
			self.blobs[v] = nil
		end
	end

	-- have any blobs appeared or need updating?
	for _, w in ipairs(self.new_blobs) do
		local found = false
		for _, v in ipairs(self.old_blobs) do
			if w == v then
				found = true
				break
			end
		end
		local b = self.blobs[w]
		if found then
			self.cb(time, '/set', 'iffff', b[1], b[4], b[5], b[6], b[7])
		else
			self.cb(time, '/on', 'iiiffff', unpack(b))
		end
	end
end

local tuio2_fltr = osc_responder:new({
	_init = function(self, cb)
		self.ignore = false
		self.last_fid = 0
		self.last_time = 0

		self.blobs = {}
		self.cb = cb
	end,

	_deinit = function(self)
		self.cb:close()
	end,

	close = function(self)
		self:_deinit()
	end,

	_root = {
		tuio2 = {
			frm = tuio2_frm,
			tok = tuio2_tok,
			alv = tuio2_alv
		}
	}
})

return tuio2_fltr
