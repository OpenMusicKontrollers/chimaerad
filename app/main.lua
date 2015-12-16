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

		self:next_job()

		return true
	end,

	error = function(self, time, uid, target, err)
		--TODO
		return true
	end,

	dump = function(self, time, fid, blob)
		if #self._sensors > 0 then
			sensors = {}
			for i=1, #blob, 2 do
				local idx = (i+1)/2
				local val = (blob[i-1] << 8) | blob[i]
				sensors[idx] = val > 0x800 and -(0xffff - val) or val
			end

			for _, v in ipairs(self._sensors) do
				v.httpd:unicast_json(v.client, {status='success', key='sensors', value=sensors})
			end
			self._sensors = {}
		end

		return true
	end,

	stream = {
		resolve = function(self)
			print('resolve')
			self:next_job()

			return true
		end,
		timeout = function(self)
			print('timeout')

			return true
		end,
		connect = function(self)
			print('connect')
			self:next_job()

			return true
		end,
		disconnect = function(self)
			print('disconnect')

			return true
		end,
		error = function(self, err)
			print('error', err)

			return true
		end
	}
}

local device = osc_responder:new({
	fullname = nil,
	version = nil,
	port = nil,

	_init = function(self)
		self._root = methods
		self._jobs = {}
		self._sensors = {}

		-- create OSC-type url for device
		self.url = {
			conf = string.gsub(self.fullname, '([^.]+)._osc._([^.]+).local', function(name, prot)
				prot = prot .. (self.version == 'inet' and 4 or 6)
				return string.format('osc.%s://%s.local:%i', prot, name, self.port)
			end),
			data = 'osc.udp4://:3333' --TODO
		}

		-- create OSC responders
		self.io = {
			conf = OSC.new(self.url.conf, self),
			data = OSC.new(self.url.data, self)
		}
	end,

	_deinit = function(self)
		-- close OSC responders
		if self.io and self.io.conf then self.io.conf:close() end
		if self.io and self.io.data then self.io.data:close() end
	end,

	next_job = function(self)
		for path, job in pairs(self._jobs) do
			print('job', path, job.format, job.value)
			if job.format then
				if job.value == true then
					job.value = 1
				elseif job.value == false then
					job.value = 0
				end
				self.io.conf(1, path, 'i' .. job.format, 13, job.value)
			else
				self.io.conf(1, path, 'i', 13)
			end

			break; -- only handle one job at a time
		end
	end,

	close = function(self)
		self:_deinit()
	end
})

local app = class:new({
	_init = function(self)
		self.discover = {}
		self.devices = {}

		-- MIDI out
		self.midi = RTMIDI.new('ChimaeraD')
		print(self.midi:virtual_port_open('MPE'))

		-- HTTPD
		self.httpd = httpd:new({
			port = 8080,

			_rest = { api = { v1 = {
				keepalive = function(httpd, client)
					httpd:push_client(client)
				end,

				interfaces = function(httpd, client)
					httpd:unicast_json(client, {status='success', key='interfaces', value=IFACE.list()})
				end,

				ports = function(httpd, client)
					httpd:unicast_json(client, {status='success', key='ports', value=self.midi:list()})
				end,

				backends = function(httpd, client)
					httpd:unicast_json(client, {status='success', key='backends', value=self.midi:backend()})
				end,

				devices = function(httpd, client)
					httpd:unicast_json(client, {status='success', key='devices', value=self.discover})
				end,

				query = function(httpd, client, data)
					local dev, j = self:find(httpd, client, data)
					if not dev then return end

					dev._jobs[j.path .. '!'] = {
						httpd = httpd,
						client = client
					}
					dev:next_job()
				end,

				get = function(httpd, client, data)
					local dev, j = self:find(httpd, client, data)
					if not dev then return end

					dev._jobs[j.path] = {
						httpd = httpd,
						client = client
					}
					dev:next_job()
				end,

				set = function(httpd, client, data)
					local dev, j = self:find(httpd, client, data)
					if not dev then return end

					dev._jobs[j.path] = {
						httpd = httpd,
						client = client,
						format = j.format,
						value = j.value
					}
					dev:next_job()
				end,

				call = function(httpd, client, data)
					local dev, j = self:find(httpd, client, data)
					if not dev then return end

					dev._jobs[j.path] = {
						httpd = httpd,
						client = client
					}
					dev:next_job()
				end,

				sensors = function(httpd, client, data)
					local dev, j = self:find(httpd, client, data)
					if not dev then return end

					table.insert(dev._sensors, {
						httpd = httpd,
						client = client
					})
				end
			} } }
		})

		-- listen to zeroconf updates
		self.dns_sd = dns_sd:new({}, function(discover)
			for k, dev in pairs(self.devices) do
				-- close device
				dev:close()
			end

			self.devices = {}
			for k, v in pairs(discover) do
				-- add device to device list
				local dev = device:new({
					fullname = k,
					version = v.version,
					port = v.port
				})
				self.devices[k] = dev
			end

			-- notify connected http clients about changes
			self.discover = discover
			self.httpd:broadcast_json({status='success', key='devices', value=self.discover})
		end)
	end,

	find = function(self, httpd, client, data)
		local err, j = JSON.decode(data.body)
		if err then
			httpd:unicast_json(client, {status='error', value='JSON decode failed'})
			return
		end

		local dev = self.devices[j.url]
		if not dev then
			httpd:unicast_json(client, {status='error', value='device non existent'})
			return
		end

		return dev, j
	end
})

local chimaerad = app:new({})

return chimaerad
