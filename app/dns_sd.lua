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

local class = require('class')

--[[
local uri
if(reply.fullname:find('_osc._udp.')) then
	uri = string.format('osc.udp://%s:%i', reply.target, reply.port)
else
	uri = string.format('osc.tcp://%s:%i', reply.target, reply.port)
end
--]]

local dns_sd = class:new({
	init = function(self, callback)
		self.db = {}
		self.resolve = {}
		self.query = {}

		self.query_cb = function(err, reply)
			if(err) then return end

			for k, v in pairs(reply) do
				--print(k, v)
				self.db[reply.target][k] = self.db[reply.target][k] or v
			end
			--print()

			if(callback) then
				callback(self.db)
			end

			self.query[reply.target] = nil
		end

		self.resolve_cb = function(err, reply)
			if(err) then return end

			--for k, v in pairs(reply) do
			--	print(k, v)
			--end
			--print()

			if(reply.txt and reply.txt.uri and reply.txt.uri == 'http://open-music-kontrollers.ch/chimaera') then
				self.db[reply.target] = reply
				self.query[reply.target] = DNS_SD.query(reply, self.query_cb)
			else
				self.db[reply.target] = nil
			end

			self.resolve[reply.target] = nil
		end

		self.browse_cb = function(err, reply)
			if(err) then return end

			local target = reply.name .. '.' .. reply.domain
			local fullname = reply.name .. '.' .. reply.type .. '.' .. reply.domain

			--for k, v in pairs(reply) do
			--	print(k, v)
			--end
			--print()

			if(reply.add) then
				self.db[target] = {}
				self.resolve[target] = DNS_SD.resolve(reply, self.resolve_cb)
				self.query[target] = nil
			else
				self.db[target] = nil
				self.resolve[target] = nil
				self.query[target] = nil

				if(callback) then
					callback(self.db)
				end
			end
		end

		self.browse = {
			udp = DNS_SD.browse({type='_osc._udp.', domain='local.'}, self.browse_cb),
			tcp = DNS_SD.browse({type='_osc._tcp.', domain='local.'}, self.browse_cb)
		}
	end
})

return dns_sd
