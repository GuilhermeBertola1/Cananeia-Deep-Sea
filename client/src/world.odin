package game

// Geração do oceano. É a MESMA conta de server/components/game/world.c,
// só com inteiros, para o cliente (WASM) e o ESP32 chegarem no mesmo mapa.
// O servidor manda o checksum no S_WELCOME e o cliente confere.

// 65536 x 65536 células, mas só os chunks perto do barco são gerados.
MAP_W :: 65536
MAP_H :: 65536

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

// Biomas por anel em volta do porto, igual ao world.c: a costa (anéis 0 a 3),
// o mar distante e, do anel FAR_RING em diante, o mar do abismo.
@(private = "file")
RING_BIOMES := [4][4]Biome {
	{.Bay, .Bay, .Bay, .Bay},
	{.Bay, .Mangrove, .Islands, .Islands},
	{.Mangrove, .Islands, .Open, .Reef},
	{.Open, .Reef, .Abyss, .Abyss},
}
@(private = "file")
FAR_RING :: 16
@(private = "file")
OUTER_BIOMES := [8]Biome{.Open, .Open, .Reef, .Islands, .Islands, .Mangrove, .Abyss, .Abyss}
@(private = "file")
FAR_BIOMES := [8]Biome{.Open, .Reef, .Islands, .Abyss, .Abyss, .Abyss, .Abyss, .Abyss}

world_region_of :: proc(v: i32) -> i32 {return (v + REGION_CELLS / 2) / REGION_CELLS}

world_region_biome :: proc(rx, ry: i32, seed: u32) -> Biome {
	ring := max(abs(rx - PORT_RX), abs(ry - PORT_RY))
	h := world_hash(rx, ry, seed ~ 0xB10E)
	if ring < len(RING_BIOMES) do return RING_BIOMES[ring][h % 4]
	if ring < FAR_RING do return OUTER_BIOMES[h % 8]
	return FAR_BIOMES[h % 8]
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

// ---------------------------------------------------------------- canais

// Igual ao world.c: cada divisa tem um ponto de passagem e um canal reto liga
// o centro da região aos pontos dos seus 4 lados.
@(private = "file")
CHANNEL_JITTER :: 32
@(private = "file")
CHANNEL_WIDTH2 :: 2

// Prende v dentro do mapa, longe 2 células da borda.
@(private = "file")
clamp_in :: proc(v, size: i32) -> i32 {return clamp(v, 2, size - 3)}

// Ponto de passagem do lado direito (side 0) ou de baixo (side 1) da região.
@(private = "file")
passage :: proc(rx, ry: i32, side: int, seed: u32) -> (px, py: i32) {
	sx: i32 = 1 if side == 0 else 0
	sy: i32 = 1 if side == 1 else 0
	h := world_hash(rx * 2 + sx, ry * 2 + sy, seed ~ 0xCA7A1)
	jitter := i32(h % (2 * CHANNEL_JITTER + 1)) - CHANNEL_JITTER
	if side == 0 {
		return rx * REGION_CELLS + REGION_CELLS / 2, clamp_in(ry * REGION_CELLS + jitter, MAP_H)
	}
	return clamp_in(rx * REGION_CELLS + jitter, MAP_W), ry * REGION_CELLS + REGION_CELLS / 2
}

// Distância² de (x, y) até o segmento de (ax, ay) a (bx, by).
@(private = "file")
dist2_segment :: proc(x, y, ax, ay, bx, by: i32) -> i32 {
	vx, vy := bx - ax, by - ay
	len2 := vx * vx + vy * vy
	t := clamp((x - ax) * vx + (y - ay) * vy, 0, len2)
	cx, cy := ax, ay
	if len2 != 0 {
		cx += vx * t / len2
		cy += vy * t / len2
	}
	return (x - cx) * (x - cx) + (y - cy) * (y - cy)
}

@(private = "file")
on_channel :: proc(x, y: i32, seed: u32) -> bool {
	rx, ry := world_region_of(x), world_region_of(y)
	cx, cy := rx * REGION_CELLS, ry * REGION_CELLS
	// Direita e baixo são desta região; esquerda e cima são da vizinha.
	if rx + 1 < REGIONS_W {
		px, py := passage(rx, ry, 0, seed)
		if dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2 do return true
	}
	if ry + 1 < REGIONS_H {
		px, py := passage(rx, ry, 1, seed)
		if dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2 do return true
	}
	if rx > 0 {
		px, py := passage(rx - 1, ry, 0, seed)
		if dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2 do return true
	}
	if ry > 0 {
		px, py := passage(rx, ry - 1, 1, seed)
		if dist2_segment(x, y, cx, cy, px, py) <= CHANNEL_WIDTH2 do return true
	}
	return false
}

world_tile_at :: proc(x, y: i32, seed: u32) -> Tile {
	if x < 0 || y < 0 || x >= MAP_W || y >= MAP_H do return .Mountain
	if x == DOCK_X && y == DOCK_Y do return .Dock

	// Ilha-porto e águas calmas em volta dela. Olha o quadrado antes de
	// calcular dx² + dy²: longe do porto a conta passaria de 32 bits.
	dx, dy := x - PORT_X, y - PORT_Y
	if abs(dx) <= 21 && abs(dy) <= 21 {
		d2 := dx * dx + dy * dy
		if d2 <= PORT_RADIUS * PORT_RADIUS {
			if d2 > 30 do return .Sand
			if d2 < 4 do return .Forest
			return .Grass
		}
		if d2 <= 10 * 10 do return .Shallow
		if d2 <= 21 * 21 do return .Water
	}

	// O bioma da região decide a média e a força do ruído.
	bp := blended_params(x, y, seed)
	e := ONE / 2 + (fbm(x, y, seed) - ONE / 2) * bp.amp / 100 + bp.bias
	switch {
	case e < 26214: return .Deep
	case e < 36045: return .Water
	case e < 39322: return .Shallow
	case on_channel(x, y, seed): return .Shallow // terra no caminho: o canal corta como um rio
	case e < 40632: return .Sand
	case e < 43909: return .Grass
	case e < 47186: return .Forest
	}
	return .Mountain
}

// Retângulo [x0, x1) × [y0, y1) da região, já cortado na borda do mapa.
world_region_rect :: proc(rx, ry: i32) -> (x0, y0, x1, y1: i32) {
	x0 = rx * REGION_CELLS - REGION_CELLS / 2
	y0 = ry * REGION_CELLS - REGION_CELLS / 2
	return max(x0, 0), max(y0, 0), min(x0 + REGION_CELLS, MAP_W), min(y0 + REGION_CELLS, MAP_H)
}

// FNV-1a da região, gerada na hora: igual ao world_region_checksum() do servidor.
world_region_checksum :: proc(rx, ry: i32, seed: u32) -> u32 {
	x0, y0, x1, y1 := world_region_rect(rx, ry)
	h: u32 = 2166136261
	for y in y0 ..< y1 {
		for x in x0 ..< x1 do h = (h ~ u32(world_tile_at(x, y, seed))) * 16777619
	}
	return h
}

tile_is_water :: proc(t: Tile) -> bool {return t <= .Shallow}
tile_is_sailable :: proc(t: Tile) -> bool {return t <= .Shallow || t == .Dock}
