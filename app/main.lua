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
local osc_responder = require('osc_responder')

local clients = {}

local function next_job(self)
	if not self._connected then return end

	for path, job in pairs(self._jobs) do
		if job.format and job.value then
			self._io(1, path, 'i' .. job.format, 13, job.value)
		else
			self._io(1, path, 'i', 13)
		end

		break; -- only handle one job at a time
	end
end

local methods = {
	success = function(self, time, uid, target, data)
		local job = self._jobs[target]
		if job then
			if string.match(target, '!') then
				-- /api/v1/query
				local err, j = JSON.decode(data)
				job.httpd:unicast_json(job.client, {status='success', key='contents', value=j})
			else
				-- /api/v1/[get,set,call]
				job.httpd:unicast_json(job.client, {status='success', key='contents', value=data})
			end
			self._jobs[target] = nil
		end
		
		next_job(self)
	end,
	error = function(self, time, uid, target, err)
		--TODO
	end,

	stream = {
		resolve = function(self)
			print('resolve')
			self._connected = true
			next_job(self)
		end,
		timeout = function(self)
			print('timeout')
			self._connected = false
		end,
		connect = function(self)
			print('connect')
			self._connected = true
			next_job(self)
		end,
		disconnect = function(self)
			print('disconnect')
			self._connected = false
		end
	}
}

local app = class:new({
	_init = function(self)
		self.devices = {}

		self.httpd = httpd:new({
			port = 8080,

			_rest = { api = { v1 = {
				keepalive = function(httpd, client)
					httpd:push_client(client)
				end,

				interfaces = function(httpd, client)
					httpd:unicast_json(client, {status='success', key='interfaces', value=IFACE.list()})
				end,

				devices = function(httpd, client)
					httpd:unicast_json(client, {status='success', key='devices', value=self.devices})
				end,

				query = function(httpd, client, data)
					local err, j = JSON.decode(data.body) -- TODO handle rrr
					local dev = clients[j.url]
					if not dev then
						dev = {
							_root = methods,
							_jobs = {},
							_connected = false
						}
						dev._io = OSC.new(j.url, osc_responder:new(dev))
						clients[j.url] = dev
					end

					dev._jobs[j.path .. '!'] = {
						httpd = httpd,
						client = client
					}

					next_job(dev)
				end,

				get = function(httpd, client, data)
					local err, j = JSON.decode(data.body) -- TODO handle rrr
					local dev = clients[j.url]

					dev._jobs[j.path] = {
						httpd = httpd,
						client = client
					}

					next_job(dev)
				end,

				set = function(httpd, client, data)
					local err, j = JSON.decode(data.body)
					local dev = clients[j.url]

					dev._jobs[j.path] = {
						httpd = httpd,
						client = client,
						format = j.format,
						value = j.value
					}

					next_job(dev)
				end,

				call = function(httpd, client, data)
					local err, j = JSON.decode(data.body)
					local dev = client[j.url]

					dev._jobs[j.path] = {
						httpd = httpd,
						client = client
					}

					next_job(dev)
				end
			} } }
		})

		self.dns_sd = dns_sd:new({}, function(devices)
			self.devices = devices
			self.httpd:broadcast_json({status='success', key='devices', value=self.devices})
		end)
	end,
})

local chimaerad = app:new({})

return chimaerad
