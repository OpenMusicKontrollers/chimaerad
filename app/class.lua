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

local class = {
	new = function(self, o, ...)
		o = o or {}

		self.__index = self

		if self._call then
			self.__call = function(o, ...)
				return self._call(o, ...)
			end
		end

		if self._deinit then
			self.__gc = function(o)
				return self._deinit(o)
			end
		end

		setmetatable(o, self)

		if self._init then
			self._init(o, ...)
		end

		return o
	end
}

return class


