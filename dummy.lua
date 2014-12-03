bit32 = bit32 or bit

gids = {}
keys = {}

n = 160/3
control = 0x07 -- volume

function on(time, sid, gid, pid, x, z)
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
end

function off(time, sid)
	local gid = gids[sid]
	local key = keys[sid]

	midi(
		bit32.bor(0x80, gid),
		key,
		0x7f)

	gids[sid] = nil
	keys[sid] = nil
end

function set(time, sid, x, z)
	local X = x*n + 23.166
	local gid = gids[sid]
	local key = keys[sid]
	local bend = (X-key)/n*0x2000 + 0x1fff
	local eff = z*0x3fff
	local eff_msb = bit32.rshift(eff, 7)
	local eff_lsb = bit32.band(eff, 0x7f)

	midi(
		bit32.bor(0xe0, gid),
		bit32.band(bend, 0x7f),
		bit32.rshift(bend, 7))
	midi(
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
end

function idle(time)
	--
end
