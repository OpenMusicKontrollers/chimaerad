function on(time, sid, gid, pid, x, z)
	print('on', sid, gid, pid, x, z)
end

function off(time, sid)
	print('off', sid)
end

function set(time, sid, x, z)
	print('set', sid, x, z)
end

function idle()
	print('idle')
end
