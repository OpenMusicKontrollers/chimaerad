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
	port = 9000,

	init = function(self)
		self.db = {}
		self.ifaces = IFACE.list()

		self.httpd = httpd:new({
			port = port,
			
			['/dns_sd/browse'] = function(httpd, client)
				print('/dns_sd/browse method')

				for _, v in ipairs(self.ifaces) do
					for k, w in pairs(self.db) do
						if(v.version == w.version) then
							w.reachable = IFACE.check(v.version, v.address, v.netmask, w.address)
						end
					end
				end
				httpd:json(client, {success=true, reply={request='/dns_sd/browse', data=self.db}})
			end,

			['/ifaces/list'] = function(httpd, client)
				print('/ifaces/list method')
				httpd:json(client, {success=true, reply={request='/ifaces/list', data=self.ifaces}})
			end
		})

		self.dns_sd = dns_sd:new({}, function(db)
			print('dns_sd callback')
			self.db = db
			self.httpd:push({success=true, reply={request='/dns_sd/browse'}})
		end)
	end
})

local app = chimaerad:new({})

return app
