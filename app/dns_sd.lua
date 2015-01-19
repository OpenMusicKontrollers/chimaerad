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

			--TODO

			self.query[reply.target] = nil
		end

		self.resolve_cb = function(err, reply)
			if(err) then return end

			if(reply.txt.uri == 'http://open-music-kontrollers.ch/chimaera') then
				self.db[reply.fullname] = reply
				self.query[reply.target] = DNS_SD.query(reply, self.query_cb)
			
				if(callback) then
					callback(self.db)
				end
			else
				self.db[reply.fullname] = nil
			end

			self.resolve[reply.fullname] = nil
		end

		self.browse_cb = function(err, reply)
			if(err) then return end

			local fullname = reply.name .. '.' .. reply.type .. reply.domain

			if(reply.add) then
				self.db[fullname] = {}
				self.resolve[fullname] = DNS_SD.resolve(reply, self.resolve_cb)
				self.query[fullname] = nil
			else
				self.db[fullname] = nil
				self.resolve[fullname] = nil
				self.query[fullname] = nil

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
