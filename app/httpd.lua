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

local code = {
	[200] = 'HTTP/1.1 200 OK\r\n',
	[404] = 'HTTP/1.1 404 Not Found\r\n'
}

local content_type = {
	html = 'Content-Type: text/html\r\n\r\n',
	json = 'Content-Type: text/json\r\n\r\n',
	css = 'Content-Type: text/css\r\n\r\n',
	js = 'Content-Type: text/javascript\r\n\r\n',
	png = 'Content-Type: image/png\r\n\r\n'
}

setmetatable(content_type, {
	__index = function(self, key)
		return 'Content-Type: application/octet-stream\r\n\r\n'
	end})


local httpd = class:new({
	-- global
	port = 8080,

	-- private
	clients = nil,
	queue = nil,

	push = function(self, data)
		table.insert(self.queue, data)
		self:dispatch()
	end,

	dispatch = function(self)
		if(#self.queue>0 and #self.clients>0) then
			local item = table.remove(self.queue, 1)

			for _, client in ipairs(self.clients) do
				local err, str = JSON.encode(item)
				if(not err) then
					client(code[200] .. content_type['json'] .. str)
				else
					client(code[200] .. content_type['json'] .. JSON.encode({success=false, error='JSON encoding'}))
				end
			end
			self.clients = {}
		end
	end,

	json = function(self, client, data)
		local err, str =  JSON.encode(data)
		if(not err) then
			client(code[200] .. content_type['json'] .. str)
		else
			client(code[200] .. content_type['json'] .. JSON.encode({success=false, error='JSON encoding'}))
		end
	end,

	['/keepalive'] = function(self, client)
		table.insert(self.clients, client)
		self:dispatch()
	end,

	init = function(self)
		self.queue = {}
		self.clients = {}

		self.io = HTTP.new(self.port, function(client, data)
			if(data.url == '/?') then
				if(data.body) then
					local err, json = JSON.decode(data.body)
					if(not err) then
						local meth = self[json.request]

						if(meth) then
							meth(self, client, json.data)
						else
							client(code[200] .. content_type['json'] .. JSON.encode({success=false, error='method missing'}))
						end
					else
						client(code[200] .. content_type['json'] .. JSON.encode({success=false, error='JSON decoding'}))
					end
				else
					client(code[200] .. content_type['json'] .. JSON.encode({success=false, error='body missing'}))
				end

			else
				if(data.url == '/') then
					data.url = '/index.html'
				end

				local index = data.url:find('%.[^%.]*$')
				local file = data.url:sub(2, index-1)
				local suffix = data.url:sub(index+1)

				local chunk = ZIP.read(file .. '.' .. suffix)
				if(chunk) then
					client(code[200] .. content_type[suffix] .. chunk)
				else
					client(code[404])
				end
			end
		end)
	end
})

return httpd
