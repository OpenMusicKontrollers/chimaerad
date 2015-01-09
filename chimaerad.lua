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

o = {
	hello = 'world'
}

s = JSON.serialize(o)
print(s)

t = JSON.deserialize(s)
print(o.hello == t.hello)

resolver = nil
resolver = OSC.new('osc.udp4://255.255.255.255:4444', function(time, path, fmt, ...)
	--print(time, path, fmt, ...)

	local methods = {
		['/stream/resolve'] = function(...)
			resolver(0, '/chimaera/discover', 'i', 13)	
		end,

		['/success'] = function(id, dest, ...)
			print('discover', ...)
		end
	}

	local meth = methods[path]
	if(meth) then meth(time, ...) end
end)

sender = nil
sender = OSC.new('osc.udp4://chimaera.local:4444', function(time, path, fmt, ...)
	--print(time, path, fmt, ...)

	local methods = {
		['/stream/resolve'] = function(...)
			sender(0, '/engines/enabled', 'ii', 13, 0)
			sender(0, '/engines/server', 'ii', 13, 0)
			sender(0, '/engines/mode', 'is', 13, 'osc.tcp')
			sender(0, '/engines/enabled', 'ii', 13, 1)

			sender(0, '/engines/reset', 'i', 13)
			sender(0, '/engines/dummy/enabled', 'ii', 13, 1)
			sender(0, '/engines/dummy/redundancy', 'ii', 13, 0)
			sender(0, '/engines/dummy/derivatives', 'ii', 13, 1)
		end,

		['/success'] = function(id, dest, ...)
			print('success', id, dest)
		end
	}

	local meth = methods[path]
	if(meth) then meth(time, ...) end
end)

responder = nil
responder = OSC.new('osc.tcp4://:3333', function(time, path, fmt, ...)
	--print(time, path, fmt, ...)

	local methods = {
		['/stream/resolve'] = function(...)
			--
		end,

		['/on'] = function(time, sid, gid, pid, x, z, X, Z)
			--
		end,

		['/set'] = function(time, sid, x, z, X, Z)
			--
		end,

		['/off'] = function(time, sid)
			--
		end,

		['/idle'] = function(time)
			--
		end
	}

	local meth = methods[path]
	if(meth) then meth(time, ...) end
end)

zip = ZIP.new('app.zip')
print(zip('COPYING'))

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

http = HTTP.new(9000, function(url)
	print(url)

	if(url:find('/%?')) then
		return code[200] .. content_type['json'] .. '{"success":false}'
	else
		if(url == '/') then
			url = '/index.html'
		end

		local index = url:find('%.[^%.]*$')
		local file = url:sub(2, index-1)
		local suffix = url:sub(index+1)
		print(file, suffix)

		local chunk = zip(file .. '.' .. suffix)
		if(chunk) then
			return code[200] .. content_type[suffix] .. chunk
		else
			return code[404]
		end
	end
end)
