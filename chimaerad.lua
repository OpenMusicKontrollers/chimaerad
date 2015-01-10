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

bit32 = bit32 or bit

id = coroutine.wrap(function()
	local val = math.random(1024)
	while true do
		coroutine.yield(val)
		val = val + 1
	end
end)

resolver = OSC.new('osc.udp4://255.255.255.255:4444', function(time, path, fmt, ...)
	--print(time, path, fmt, ...)

	local methods = {
		['/stream/resolve'] = function(...)
			resolver(0, '/chimaera/discover', 'i', id())	
		end,
		['/stream/timeout'] = function(...)
			resolver = nil
		end,

		['/success'] = function(id, dest, ...)
			print('discover', ...)
		end
	}

	local meth = methods[path]
	if(meth) then meth(time, ...) end
end)

sender = OSC.new('osc.udp4://chimaera.local:4444', function(time, path, fmt, ...)
	--print(time, path, fmt, ...)

	local introspect = {}

	local methods = {
		['/stream/resolve'] = function(...)
			sender(0, '/engines/enabled', 'ii', id(), 0)
			sender(0, '/engines/server', 'ii', id(), 0)
			sender(0, '/engines/mode', 'is', id(), 'osc.tcp')
			sender(0, '/engines/enabled', 'ii', id(), 1)

			sender(0, '/engines/reset', 'i', id())
			sender(0, '/engines/dummy/enabled', 'ii', id(), 1)
			sender(0, '/engines/dummy/redundancy', 'ii', id(), 0)
			sender(0, '/engines/dummy/derivatives', 'ii', id(), 0)

			sender(0, '/!', 'i', id())
		end,
		['/stream/timeout'] = function(...)
			sender = nil
		end,

		['/success'] = function(time, uuid, dest, ...)
			print('success', uuid, dest)

			if(dest:sub(-1) == '!') then
				local o = JSON.decode(...)
				introspect[o.path] = o
				if(o.type == 'node') then
					for _, v in ipairs(o.items) do
						sender(0, o.path .. v .. '!', 'i', id())
					end
				end
			end
		end
	}

	local meth = methods[path]
	if(meth) then meth(time, ...) end
end)

midi = RTMIDI.new('UNIX_JACK')
--midi = RTMIDI.new('LINUX_ALSA')
midi:open_virtual_port()
--mid:close_port()

local gids = {}
local keys = {}
local n = 160/3
local control = 0x07 -- volume

responder = OSC.new('osc.tcp4://:3333', function(time, path, fmt, ...)
	--print(time, path, fmt, ...)

	local methods = {
		['/stream/resolve'] = function(...)
			--
		end,
		['/stream/timeout'] = function(...)
			responder = nil
		end,

		['/on'] = function(time, sid, gid, pid, x, z)
			local X = x*n + 23.166
			local key = math.floor(X)
			local bend = (X-key)/n*0x2000 + 0x1fff
			local eff = z*0x3fff
			local eff_msb = bit32.rshift(eff, 7)
			local eff_lsb = bit32.band(eff, 0x7f)

			midi( -- note on
				bit32.bor(0x90, gid),
				key,
				0x7f)
			midi( -- pitch bend
				bit32.bor(0xe0, gid),
				bit32.band(bend, 0x7f),
				bit32.rshift(bend, 7))
			midi( -- note pressure
				bit32.bor(0xa0, gid),
				key,
				eff_msb)
			midi( -- control change
				bit32.bor(0xb0, gid),
				bit32.bor(0x20, control),
				eff_lsb)
			midi( -- control change
				bit32.bor(0xb0, gid),
				control,
				eff_msb)
			
			gids[sid] = gid
			keys[sid] = key
		end,

		['/off'] = function(time, sid)
			local gid = gids[sid]
			local key = keys[sid]

			midi( -- note off
				bit32.bor(0x80, gid),
				key,
				0x7f)

			gids[sid] = nil
			keys[sid] = nil
		end,

		['/set'] = function(time, sid, x, z)
			local X = x*n + 23.166
			local gid = gids[sid]
			local key = keys[sid]
			local bend = (X-key)/n*0x2000 + 0x1fff
			local eff = z*0x3fff
			local eff_msb = bit32.rshift(eff, 7)
			local eff_lsb = bit32.band(eff, 0x7f)

			midi( -- pitch bend
				bit32.bor(0xe0, gid),
				bit32.band(bend, 0x7f),
				bit32.rshift(bend, 7))
			midi( -- note pressure
				bit32.bor(0xa0, gid),
				key,
				eff_msb)
			midi( -- control change
				bit32.bor(0xb0, gid),
				bit32.bor(0x20, control),
				eff_lsb)
			midi( -- control change
				bit32.bor(0xb0, gid),
				control,
				eff_msb)
		end,

		['/idle'] = function(time)
			--
		end
	}

	local meth = methods[path]
	if(meth) then meth(time, ...) end
end)

zip = ZIP.new('app.zip')

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
	if(url:find('/%?')) then
		return code[200] .. content_type['json'] .. '{"success":false}'
	else
		if(url == '/') then
			url = '/index.html'
		end

		local index = url:find('%.[^%.]*$')
		local file = url:sub(2, index-1)
		local suffix = url:sub(index+1)

		local chunk = zip(file .. '.' .. suffix)
		if(chunk) then
			return code[200] .. content_type[suffix] .. chunk
		else
			return code[404]
		end
	end
end)
