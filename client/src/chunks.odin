package game

// Cache de chunks do cliente: o mapa em pedaços de 64x64 células, gerados com
// world_tile_at() quando alguém pede um tile deles e esquecidos quando o cache
// enche (sai o que foi usado há mais tempo). O servidor faz a mesma coisa em
// chunks.c, só que lá nada é esquecido.

CHUNK_SHIFT :: 6
CHUNK_CELLS :: 1 << CHUNK_SHIFT // 64
CHUNK_MAX :: 512 // 512 chunks × 4 KB = 2 MB, bem mais que a tela precisa

Chunk :: struct {
	cx, cy: i32, // posição do chunk (célula / 64)
	used:   u32, // quadro em que foi usado pela última vez
	tiles:  [CHUNK_CELLS * CHUNK_CELLS]Tile,
}

@(private = "file")
cache: struct {
	chunks: [CHUNK_MAX]Chunk,
	count:  int,
	index:  map[[2]i32]int, // (cx, cy) -> posição em chunks
	frame:  u32,
	last:   int, // último chunk usado: quase sempre é o mesmo da próxima vez
	seed:   u32,
}

// Esquece todos os chunks (semente nova).
chunks_reset :: proc(seed: u32) {
	clear(&cache.index)
	cache.count = 0
	cache.last = -1
	cache.seed = seed
}

// Chamar uma vez por quadro: é o relógio do "usado há mais tempo".
chunks_next_frame :: proc() {cache.frame += 1}

chunks_count :: proc() -> int {return cache.count}

chunks_tile :: proc(x, y: i32) -> Tile {
	if x < 0 || y < 0 || x >= MAP_W || y >= MAP_H do return .Mountain
	cx, cy := x >> CHUNK_SHIFT, y >> CHUNK_SHIFT
	i := cache.last
	if i < 0 || i >= cache.count || cache.chunks[i].cx != cx || cache.chunks[i].cy != cy {
		ok: bool
		i, ok = cache.index[[2]i32{cx, cy}]
		if !ok do i = chunk_create(cx, cy)
		cache.last = i
	}
	c := &cache.chunks[i]
	c.used = cache.frame
	return c.tiles[(y & (CHUNK_CELLS - 1)) * CHUNK_CELLS + (x & (CHUNK_CELLS - 1))]
}

@(private = "file")
chunk_create :: proc(cx, cy: i32) -> int {
	i := cache.count
	if i < CHUNK_MAX {
		cache.count += 1
	} else {
		// Cache cheio: reaproveita o chunk usado há mais tempo.
		i = 0
		for j in 1 ..< CHUNK_MAX {
			if cache.chunks[j].used < cache.chunks[i].used do i = j
		}
		old := &cache.chunks[i]
		delete_key(&cache.index, [2]i32{old.cx, old.cy})
	}
	c := &cache.chunks[i]
	c.cx, c.cy = cx, cy
	for y in i32(0) ..< CHUNK_CELLS {
		for x in i32(0) ..< CHUNK_CELLS {
			c.tiles[y * CHUNK_CELLS + x] = world_tile_at(cx * CHUNK_CELLS + x, cy * CHUNK_CELLS + y, cache.seed)
		}
	}
	cache.index[[2]i32{cx, cy}] = i
	return i
}
