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
local httpd = require('httpd')
local dns_sd = require('dns_sd')

local midi_out = require('midi_out')
local tuio2_fltr = require('tuio2_fltr')
local map = require('map')

local function devices_change(self, httpd, client, data)
	local w = self.db[data.target]
	local conf = self.conf[data.target]

	if(w and conf) then
		if(data.name ~= w.name) then
			conf(0, '/info/name', 'is', 13, data.name)
		end

		if(data.address ~= w.address) then
			conf(0, '/comm/ip', 'is', 14, data.address .. '/24') -- FIXME netmask
		end

		data.ipv4ll = data.dhcp or data.ipv4ll -- enable ipv4ll together with dhcp

		if(data.ipv4ll ~= w.txt.ipv4ll) then
			conf(0, '/ipv4ll/enabled', 'ii', 15, (data.ipv4ll and 1) or 0)
		end

		if(data.dhcp ~= w.txt.dhcp) then
			conf(0, '/dhcpc/enabled', 'ii', 16, (data.dhcp and 1) or 0)
		end
	end

	-- FIXME only answer after OSC has been successful
	httpd:json(client, {status='success', data=nil})
end

local function api_v1_introspection(self, httpd, client, data)
	local w = self.db[data.target]
	local conf = self.conf[data.target]

	if(w and conf) then
		conf(0, '/!', 'i', 13)
			--TODO
	end

	-- FIXME only answer after introspection has been finished
	--httpd:json(client, {status='success', data=nil})
end

local function comm_cb(self, w, time, path, fmt, ...)
	if(path == '/stream/resolve') then
		print(path, fmt, ...)
		self.conf[w.fullname](0, '/engines/enabled', 'ii', 13, 0)
		self.conf[w.fullname](0, '/engines/server', 'ii', 13, 0)
		self.conf[w.fullname](0, '/engines/mode', 'is', 13, w.mode)
		self.conf[w.fullname](0, '/engines/address', 'is', 13, 'this:3333')
		self.conf[w.fullname](0, '/engines/reset', 'i', 13)
		self.conf[w.fullname](0, '/sensors/group/attributes/0', 'iffiii', 13, 0.0, 1.0, 0, 1, 0)
		self.conf[w.fullname](0, '/sensors/group/attributes/1', 'iffiii', 13, 0.0, 1.0, 1, 0, 0)

		if(w.mode == 'osc.udp') then
			self.conf[w.fullname](0, '/engines/tuio2/derivatives', 'ii', 13, 0)
			self.conf[w.fullname](0, '/engines/tuio2/enabled', 'ii', 13, 1)
		else
			self.conf[w.fullname](0, '/engines/dummy/derivatives', 'ii', 13, 0)
			self.conf[w.fullname](0, '/engines/dummy/redundancy', 'ii', 13, 0)
			self.conf[w.fullname](0, '/engines/dummy/enabled', 'ii', 13, 1)
		end
		self.conf[w.fullname](0, '/engines/enabled', 'ii', 13, 1)

	elseif(path == '/stream/connect') then
		print(path, fmt, ...)
	end
end

local function conf_cb(self, w, time, path, fmt, uid, target, ...)
	print(path, fmt, uid, target, ...) 

	if(path == '/stream/resolve' and w.reachable) then
		self.conf[w.fullname](0, '/sensors/number', 'i', 13)
	elseif(path == '/success' and target == '/sensors/number') then
		local uri = string.format('%s://:%i', w.mode, 3333)
		local n = ...
		local md = midi_out:new({map=map_linear:new({oct=2, n=n}), control=0x4a})

		if(w.mode == 'osc.udp') then
			self.engine[w.fullname] = tuio2_fltr:new({}, md)
		else -- 'osc.tcp'
			self.engine[w.fullname] = md
		end

		self.comm[w.fullname] = OSC.new(uri, function(...)
			return self.engine[w.fullname]
				and not self.engine[w.fullname](...)
				or not comm_cb(self, w, ...)
		end)
	end
end

--[[
conf_responder = osc_responder:new({
	_root = {
		stream = {
			resolve = function(self, time, ...)
				self(0, '/sensors/number', 'i', 13)
			end,

			connect = function(self, time, ...)
				--TODO only used if conf_mode == 'osc.tcp'
			end
		},

		success = function(self, time, uid, path, ...)

		end,

		fail = function(self, time, uid, path, msg)
			print('/fail', uid, path, msg)
		end
	}
})
--]]

local function api_v1_devices(self, httpd, client)
	for k, w in pairs(self.db) do
		w.reachable = false
		for _, v in ipairs(self.ifaces) do
			if(v.version == w.version) then
				w.reachable = IFACE.check(v.version, v.address, v.netmask, w.address)
				if(w.reachable) then break end
			end
			-- FIXME make this configurable
			--w.mode = 'osc.tcp'
			w.mode = 'osc.udp'
		end
	end

	-- close all open deprecated configuration connections
	for k, w in pairs(self.conf) do
		if(not self.db[k] or self.db[k].updated) then
			w:close()
			self.conf[k] = nil
		end
	end

	-- close all open deprecated communication connections
	for k, w in pairs(self.comm) do
		if(not self.db[k] or self.db[k].updated) then
			w:close()
			self.comm[k] = nil
		end
	end

	-- close all open deprecated engines
	for k, w in pairs(self.engine) do
		if(not self.db[k] or self.db[k].updated) then
			w:close()
			self.engine[k] = nil
		end
	end

	for k, w in pairs(self.db) do
		if(not self.conf[k] or w.updated) then
			-- create OSC config hooks
			local protocol = w.version == 'inet' and 4 or 6
			local target = w.reachable and w.address or '255.255.255.255'

			local uri = string.find(k, '_osc._udp.')
				and string.format('osc.udp%i://%s:%i', protocol, target, w.port)
				or  string.format('osc.tcp%i://%s:%i', protocol, target, w.port)

			self.conf[k] = OSC.new(uri, function(...)
				conf_cb(self, w, ...)
			end)

			w.updated = nil
		end
	end

	httpd:json(client, {status='success', data=self.db})
end

local chimaerad = class:new({
	port = 8080,

	_init = function(self)
		self.ifaces = IFACE.list()
		self.db = {}
		self.conf = {}
		self.comm = {}
		self.engine = {}

		self.httpd = httpd:new({
			port = self.port,

			--[[
			['/devices/change'] = function(httpd, client, data)
				devices_change(self, httpd, client, data)
			end,
			--]]

			api_v1 = {
				['/api/v1/keepalive'] = function(httpd, client)
					httpd:add(client)
				end,

				['/api/v1/interfaces'] = function(httpd, client)
					httpd:json(client, {status='success', data=self.ifaces})
				end,

				['/api/v1/devices'] = function(httpd, client, data)
					api_v1_devices(self, httpd, client)
				end,

				['/api/v1/introspection'] = function(httpd, client, data)
					api_v1_introspection(self, httpd, client, data)
				end
			}
		})

		self.dns_sd = dns_sd:new({}, function(db)
			self.db = db
			self.httpd:push({status='success', data={href='/api/v1/devices'}})
		end)
	end
})

local app = chimaerad:new({})

return app
