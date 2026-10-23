package game

// Geração do oceano. É a MESMA conta de server/components/game/world.c,
// só com inteiros, para o cliente (WASM) e o ESP32 chegarem no mesmo mapa.
// O servidor manda o checksum no S_WELCOME e o cliente confere.

MAP_W :: 512
MAP_H :: 512

PORT_X :: MAP_W / 2
PORT_Y :: MAP_H / 2
PORT_RADIUS :: 7
DOCK_X :: PORT_X
DOCK_Y :: PORT_Y + PORT_RADIUS

Tile :: enum u8 {
	Deep,
	Water,
	Shallow,
	Sand,
	Grass,
	Forest,
	Mountain,
	Dock,
}

@(private = "file")
ONE :: 65536

world_hash :: proc(x, y: i32, seed: u32) -> u32 {
	h := u32(x) * 374761393 + u32(y) * 668265263 + seed * 2147483647
	h = (h ~ (h >> 13)) * 1274126177
	return h ~ (h >> 16)
}

// Curva suave 3t² - 2t³, em ponto fixo.
@(private = "file")
smooth :: proc(t: i32) -> i32 {
	t2 := i64(t) * i64(t) / ONE
	return i32(t2 * (3 * ONE - 2 * i64(t)) / ONE)
}

@(private = "file")
value_noise :: proc(x, y, cell: i32, seed: u32) -> i32 {
	ix, iy := x / cell, y / cell
	fx := smooth((x % cell) * ONE / cell)
	fy := smooth((y % cell) * ONE / cell)
	a := i32(world_hash(ix, iy, seed) >> 16)
	b := i32(world_hash(ix + 1, iy, seed) >> 16)
	c := i32(world_hash(ix, iy + 1, seed) >> 16)
	d := i32(world_hash(ix + 1, iy + 1, seed) >> 16)
	top := a + i32(i64(b - a) * i64(fx) / ONE)
	bottom := c + i32(i64(d - c) * i64(fx) / ONE)
	return top + i32(i64(bottom - top) * i64(fy) / ONE)
}

// Soma de 5 oitavas: células de 64, 32, 16, 8 e 4, com pesos 16, 8, 4, 2, 1.
@(private = "file")
fbm :: proc(x, y: i32, seed: u32) -> i32 {
	sum: i32
	for o in i32(0) ..< 5 {
		sum += value_noise(x, y, 64 >> u32(o), seed + u32(o) * 101) * (16 >> u32(o))
	}
	return sum / 31
}

@(private = "file")
isqrt :: proc(n: i32) -> i32 {
	r: i32
	for (r + 1) * (r + 1) <= n do r += 1
	return r
}

world_tile_at :: proc(x, y: i32, seed: u32) -> Tile {
	if x < 0 || y < 0 || x >= MAP_W || y >= MAP_H do return .Mountain
	if x == DOCK_X && y == DOCK_Y do return .Dock

	dx, dy := x - PORT_X, y - PORT_Y
	d2 := dx * dx + dy * dy
	// Ilha-porto e águas calmas em volta dela.
	if d2 <= PORT_RADIUS * PORT_RADIUS {
		if d2 > 30 do return .Sand
		if d2 < 4 do return .Forest
		return .Grass
	}
	if d2 <= 10 * 10 do return .Shallow
	if d2 <= 21 * 21 do return .Water

	// Quanto mais longe do centro, mais fundo fica o mar.
	e := fbm(x, y, seed) - isqrt(d2) * 14
	switch {
	case e < 26214: return .Deep
	case e < 36045: return .Water
	case e < 39322: return .Shallow
	case e < 40632: return .Sand
	case e < 43909: return .Grass
	case e < 47186: return .Forest
	}
	return .Mountain
}

world_generate :: proc(tiles: ^[MAP_W * MAP_H]Tile, seed: u32) {
	for y in i32(0) ..< MAP_H {
		for x in i32(0) ..< MAP_W {
			tiles[y * MAP_W + x] = world_tile_at(x, y, seed)
		}
	}
}

// FNV-1a, igual ao world_checksum do servidor.
world_checksum :: proc(tiles: ^[MAP_W * MAP_H]Tile) -> u32 {
	h: u32 = 2166136261
	for t in tiles do h = (h ~ u32(t)) * 16777619
	return h
}

tile_is_water :: proc(t: Tile) -> bool {return t <= .Shallow}
tile_is_sailable :: proc(t: Tile) -> bool {return t <= .Shallow || t == .Dock}
