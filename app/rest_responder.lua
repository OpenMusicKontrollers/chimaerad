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

local rest_responder = class:new({
	__call = function(self, url, ...)
		local root = self._rest

		for key in string.gmatch(url, '/([^/]+)') do
			root = root and root[key]
		end

		return root and not root(self, ...)
	end,

	_init = function(self)
		self.__call = getmetatable(self).__call
	end
})

return rest_responder
