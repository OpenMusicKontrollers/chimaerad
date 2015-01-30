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

local function monitor_ip_cb(self, callback, err, reply)
	if(err) then return end
	if(not reply.add) then return end
	local data = self.db[reply.fullname]
	if(not data) then return end

	for k, v in pairs(reply) do
		data[k] = v -- update entries
	end
	data.updated = true

	if(callback) then
		callback(self.db)
	end
end

local function monitor_txt_cb(self, callback, err, reply)
	if(err) then return end
	if(not reply.add) then return end
	local data = self.db[reply.fullname]
	if(not data) then return end

	for k, v in pairs(reply) do
		data[k] = v -- update entries
	end
	data.updated = true

	if(callback) then
		callback(self.db)
	end
end

local function resolve_cb(self, callback, err, reply)
	if(err) then return end

	if(reply.txt and reply.txt.uri and reply.txt.uri == 'http://open-music-kontrollers.ch/chimaera') then
		local data = self.db[reply.fullname]
		local dev =  self.dev[reply.fullname]
		if(not data) then return end
		if(not dev) then return end

		for k, v in pairs(reply) do
			data[k] = v -- update entries
		end

		dev.monitor_ip = DNS_SD.monitor_ip(reply, function(err, reply)
			monitor_ip_cb(self, callback, err, reply)
		end)

		dev.monitor_txt = DNS_SD.monitor_txt(reply, function(err, reply)
			monitor_txt_cb(self, callback, err, reply)
		end)

		dev.resolve = nil
	else -- not a chimaera controller

		device_remove(self, reply.fullname)
	end
end

local function device_remove(self, fullname)
	local dev = self.dev[fullname]

	if(not dev) then return end
	print('device_close', fullname)

	if(dev.resolve) then
		dev.resolve:close()
	end

	if(dev.monitor_ip) then
		dev.monitor_ip:close()
	end

	if(dev.monitor_txt) then
		dev.monitor_txt:close()
	end
	
	self.dev[fullname] = nil
	self.db[fullname] = nil
end

local function browse_cb(self, callback, err, reply)
	if(err) then return end

	local target = reply.name .. '.' .. reply.domain
	local fullname = reply.name .. '.' .. reply.type .. '.' .. reply.domain

	if(reply.add) then
		device_remove(self, fullname)

		self.db[fullname] = {
			name = reply.name
		}

		self.dev[fullname] = {
			resolve = DNS_SD.resolve(reply, function(err, reply)
				resolve_cb(self, callback, err, reply)
			end)
		}
	else -- not reply.add
		device_remove(self, fullname)

		if(callback) then
			callback(self.db)
		end
	end
end

local dns_sd = class:new({
	_init = function(self, callback)
		self.dev = {}
		self.db = {}

		local browse_redirect = function(err, reply)
			browse_cb(self, callback, err, reply)
		end

		self.browse_udp = DNS_SD.browse({type='_osc._udp.', domain='local.'}, browse_redirect)
		self.browse_tcp = DNS_SD.browse({type='_osc._tcp.', domain='local.'}, browse_redirect)
	end
})

return dns_sd
