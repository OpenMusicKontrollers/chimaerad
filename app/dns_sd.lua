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

local dns_sd = class:new({
	init = function(self, callback)
		self.db = {}
		self.resolve = {}
		self.monitor_ip = {}
		self.monitor_txt = {}

		self.monitor_ip_cb = function(err, reply)
			if(err) then return end

			if(reply.add) then
				for k, v in pairs(reply) do
					self.db[reply.fullname][k] = v -- update entries
				end
			end

			if(not reply.more_coming and callback) then
				callback(self.db)
			end
		end

		self.monitor_txt_cb = function(err, reply)
			if(err) then return end

			if(reply.add) then
				for k, v in pairs(reply) do
					self.db[reply.fullname][k] = v -- update entries
				end
			end

			if(not reply.more_coming and callback) then
				callback(self.db)
			end
		end

		self.resolve_cb = function(err, reply)
			if(err) then return end

			if(reply.txt and reply.txt.uri and reply.txt.uri == 'http://open-music-kontrollers.ch/chimaera') then
				for k, v in pairs(reply) do
					self.db[reply.fullname][k] = v -- update entries
				end
				self.monitor_ip[reply.fullname] = DNS_SD.monitor_ip(reply, self.monitor_ip_cb)
				self.monitor_txt[reply.fullname] = DNS_SD.monitor_txt(reply, self.monitor_txt_cb)
			else
				self.db[reply.fullname] = nil
			end

			self.resolve[reply.fullname] = nil
		end

		self.browse_cb = function(err, reply)
			if(err) then return end

			local target = reply.name .. '.' .. reply.domain
			local fullname = reply.name .. '.' .. reply.type .. '.' .. reply.domain

			if(reply.add) then
				if(self.resolve[fullname]) then
					self.resolve[fullname]:close()
				end
				if(self.monitor_ip[fullname]) then
					self.monitor_ip[fullname]:close()
				end
				if(self.monitor_txt[fullname]) then
					self.monitor_txt[fullname]:close()
				end

				self.db[fullname] = {name = reply.name}
				self.resolve[fullname] = DNS_SD.resolve(reply, self.resolve_cb)
				self.monitor_ip[fullname] = nil
				self.monitor_txt[fullname] = nil

			else -- not reply.add
				if(self.resolve[fullname]) then
					self.resolve[fullname]:close()
				end
				if(self.monitor_ip[fullname]) then
					self.monitor_ip[fullname]:close()
				end
				if(self.monitor_txt[fullname]) then
					self.monitor_txt[fullname]:close()
				end

				self.db[fullname] = nil
				self.resolve[fullname] = nil
				self.monitor_ip[fullname] = nil
				self.monitor_txt[fullname] = nil

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
