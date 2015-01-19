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
	port = 9000,

	-- private
	wait_for_json = false,
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
				client(code[200] .. content_type['json'] .. JSON.encode(item))
			end
			self.clients = {}
		end
	end,

	json = function(self, client, data)
		client(code[200] .. content_type['json'] .. JSON.encode(data))
	end,

	['/keepalive'] = function(self, client)
		table.insert(self.clients, client)
		self:dispatch()
	end,

	init = function(self)
		self.queue = {}
		self.clients = {}

		self.io = HTTP.new(self.port, function(client, event, data)
			--print(event, data)

			if(event == 'url') then
				local url = data
					
				if(url == '/?') then
					self.wait_for_json = true
				else
					if(url == '/') then
						url = '/index.html'
					end

					local index = url:find('%.[^%.]*$')
					local file = url:sub(2, index-1)
					local suffix = url:sub(index+1)

					local chunk = ZIP.read(file .. '.' .. suffix)
					if(chunk) then
						client(code[200] .. content_type[suffix] .. chunk)
					else
						client(code[404])
					end
				end
			elseif(event == 'body') then
				local body = data

				if(self.wait_for_json) then
					local json = JSON.decode(body)
	
					local meth = self[json.request]
					if(meth) then meth(self, client) end

					self.wait_for_json = false
				end
			end
		end)
	end
})

return httpd
