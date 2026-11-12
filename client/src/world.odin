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

// Regiões: igual ao world.h. A região de uma célula é a do centro mais perto.
REGION_CELLS :: 128
REGIONS_W :: MAP_W / REGION_CELLS + 1
REGIONS_H :: MAP_H / REGION_CELLS + 1
PORT_RX :: PORT_X / REGION_CELLS
PORT_RY :: PORT_Y / REGION_CELLS

// A ordem é a mesma do biome_t do servidor.
Biome :: enum u8 {
	Bay,      // Baía
	Mangrove, // Mangue
	Islands,  // Arquipélago
	Open,     // Mar aberto
	Reef,     // Recife
	Abyss,    // Abismo
}

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

// ---------------------------------------------------------------- regiões

// bias: soma na média do ruído; amp: quanto ele varia, em %. Igual ao world.c.
@(private = "file")
Biome_Params :: struct {
	bias, amp: i32,
}

@(private = "file")
BIOME_PARAMS := [Biome]Biome_Params {
	.Bay      = {2000, 70},
	.Mangrove = {5500, 50},
	.Islands  = {3000, 150},
	.Open     = {-5000, 60},
	.Reef     = {4000, 100},
	.Abyss    = {-12000, 40},
}

@(private = "file")
RING_BIOMES := [4][4]Biome {
	{.Bay, .Bay, .Bay, .Bay},
	{.Bay, .Mangrove, .Islands, .Islands},
	{.Mangrove, .Islands, .Open, .Reef},
	{.Open, .Reef, .Abyss, .Abyss},
}

world_region_of :: proc(v: i32) -> i32 {return (v + REGION_CELLS / 2) / REGION_CELLS}

world_region_biome :: proc(rx, ry: i32, seed: u32) -> Biome {
	ring := max(abs(rx - PORT_RX), abs(ry - PORT_RY))
	ring = min(ring, len(RING_BIOMES) - 1)
	h := world_hash(rx, ry, seed ~ 0xB10E)
	return RING_BIOMES[ring][h % 4]
}

@(private = "file")
mix :: proc(a, b, t: i32) -> i32 {return a + i32(i64(b - a) * i64(t) / ONE)}

@(private = "file")
blended_params :: proc(x, y: i32, seed: u32) -> Biome_Params {
	rx, ry := x / REGION_CELLS, y / REGION_CELLS
	fx := smooth((x % REGION_CELLS) * ONE / REGION_CELLS)
	fy := smooth((y % REGION_CELLS) * ONE / REGION_CELLS)
	a := BIOME_PARAMS[world_region_biome(rx, ry, seed)]
	b := BIOME_PARAMS[world_region_biome(rx + 1, ry, seed)]
	c := BIOME_PARAMS[world_region_biome(rx, ry + 1, seed)]
	d := BIOME_PARAMS[world_region_biome(rx + 1, ry + 1, seed)]
	return {
		bias = mix(mix(a.bias, b.bias, fx), mix(c.bias, d.bias, fx), fy),
		amp = mix(mix(a.amp, b.amp, fx), mix(c.amp, d.amp, fx), fy),
	}
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

	// O bioma da região decide a média e a força do ruído.
	bp := blended_params(x, y, seed)
	e := ONE / 2 + (fbm(x, y, seed) - ONE / 2) * bp.amp / 100 + bp.bias
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
