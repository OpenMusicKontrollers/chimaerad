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

local chimaerad = class:new({
	port = 8080,

	init = function(self)
		self.db = {}
		self.conf = {}
		self.ifaces = IFACE.list()

		self.httpd = httpd:new({
			port = port,

			['/devices/change'] = function(httpd, client, data)
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
				httpd:json(client, {success=true, reply={request='/devices/change'}})
			end,
			
			['/dns_sd/browse'] = function(httpd, client)
				for k, w in pairs(self.db) do
					w.reachable = false
					for _, v in ipairs(self.ifaces) do
						if(v.version == w.version) then
							w.reachable = IFACE.check(v.version, v.address, v.netmask, w.address)
							if(w.reachable) then break end
						end
					end
				end

				-- close all open configuration connections
				for k, w in pairs(self.conf) do
					if(not self.db[k]) then
						w:close()
						self.conf[k] = nil
					end
				end

				for k, w in pairs(self.db) do
					-- create OSC config hooks
					local protocol = w.version == 'inet' and 4 or 6
					local target = w.reachable and w.address or '255.255.255.255'

					local uri = string.find(k, '_osc._udp.')
						and string.format('osc.udp%i://%s:%i', protocol, target, w.port)
						or  string.format('osc.tcp%i://%s:%i', protocol, target, w.port)

					self.conf[w.fullname] = OSC.new(uri, function(time, path, fmt, ...)
						-- report status changes
						if(path == '/stream/resolve') then
							--TODO e.g. introspect
						end
					end)
				end

				httpd:json(client, {success=true, reply={request='/dns_sd/browse', data=self.db}})
			end,

			['/ifaces/list'] = function(httpd, client)
				httpd:json(client, {success=true, reply={request='/ifaces/list', data=self.ifaces}})
			end
		})

		self.dns_sd = dns_sd:new({}, function(db)
			self.db = db
			self.httpd:push({success=true, reply={request='/dns_sd/browse'}})
		end)
	end
})

local app = chimaerad:new({})

return app
