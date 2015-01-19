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

code = {
	[200] = 'HTTP/1.1 200 OK\r\n',
	[404] = 'HTTP/1.1 404 Not Found\r\n'
}

content_type = {
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

local wait_for_body = false
http = HTTP.new(9000, function(client, event, data)
	print(event, data)

	if(event == 'url') then
		local url = data
			
		if(url == '/?') then
			wait_for_body = true
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

		if(wait_for_body) then
			client(code[200] .. content_type['json'] .. '{"success":false}')
			wait_for_body = false
		end
	end
end)
