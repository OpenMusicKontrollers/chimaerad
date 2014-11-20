JSON = require('JSON')
--JSON.strictTypes = true

header = {
	html = 'HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n',
	css = 'HTTP/1.1 200 OK\r\nContent-Type: text/css\r\n\r\n',
	js = 'HTTP/1.1 200 OK\r\nContent-Type: text/javascript\r\n\r\n',
}

hash = {}

function load_file(path, header_type)
	if(not hash[path]) then
		local f = io.input(path)
		hash[path] = f:read('*a')
		f:close()
	end

	return header[header_type] .. hash[path]
end

files = {
	['/index.html'] = 'html',
	['/content.html'] = 'html',
	['/style.css'] = 'css',
	['/jquery-1.11.1.min.js'] = 'js',
}

urls = {
	['not_found'] = function(query)
		return load_file('./not_found.html', 'html')
	end,

	['/'] = function(query)
		return load_file('./index.html', 'html')
	end,

	['/osc/'] = function(query)
		query = query:gsub('%%22', '\"')
		local obj = JSON:decode(query)

		local str = header['html'] .. '<!DOCTYPE html><html><body>'
		for k, v in pairs(obj) do
			str = str .. string.format('<b>%s</b><br />', k)
			for _, w in ipairs(v) do
				for t, x in pairs(w) do
					str = str .. string.format('<i>%s</i> : %s<br />', t, tostring(x))
				end
			end
		end
		return str .. '</body></html>'
	end,
}

mt = {
	__index = function(self, key)
		if files[key] then
			return function(query) return load_file('.' .. key, files[key]) end
		else
			return rawget(self, 'not_found')
		end
	end
}
setmetatable(urls, mt)

responder = nil
query = nil

function on_url(url)
	print('on_url: ' .. url)

	local root

	local has_query = url:find('?')
	if(has_query) then
		root, query = url:match('([^?]*)?(.*)')
	else
		root, query = url, nil
	end
	--print(root, query)

	responder = urls[root]

	return 0
end

function on_headers_complete()
	print('on_headers_complete')

	if(responder) then
		return responder(query)
	else
		return -1
	end
end
