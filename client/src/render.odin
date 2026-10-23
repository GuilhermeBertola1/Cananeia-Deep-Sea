package game

// Desenho do mundo em camadas (SCOM 3): oceano, ilhas, vida marinha,
// barcos, efeitos e céu. Tudo é caractere ASCII com a fonte padrão da raylib,
// sem nenhuma textura externa.

import "core:math"
import rl "vendor:raylib"

CELL_W :: 12
CELL_H :: 20
FONT_SIZE :: 20 // 2x a fonte padrão (10 px), fica nítido
PANEL_W :: 330

COLOR_BG :: rl.Color{5, 11, 20, 255}
COLOR_PANEL :: rl.Color{11, 22, 38, 255}
COLOR_INK :: rl.Color{215, 227, 244, 255}
COLOR_QUIET :: rl.Color{123, 140, 166, 255}
COLOR_ACCENT :: rl.Color{255, 209, 102, 255}
COLOR_SCHOOL :: rl.Color{125, 249, 255, 255}

PLAYER_COLORS := [6]rl.Color {
	{255, 107, 107, 255},
	{199, 125, 255, 255},
	{244, 162, 97, 255},
	{128, 237, 153, 255},
	{255, 159, 243, 255},
	{255, 255, 255, 255},
}

Tile_Style :: struct {
	fg, bg: rl.Color,
}

TILE_STYLE := [Tile]Tile_Style {
	.Deep     = {{27, 58, 107, 255}, {6, 16, 34, 255}},
	.Water    = {{47, 111, 176, 255}, {10, 28, 56, 255}},
	.Shallow  = {{88, 180, 209, 255}, {16, 52, 74, 255}},
	.Sand     = {{233, 216, 166, 255}, {58, 52, 36, 255}},
	.Grass    = {{106, 191, 105, 255}, {20, 44, 24, 255}},
	.Forest   = {{45, 138, 78, 255}, {12, 32, 18, 255}},
	.Mountain = {{154, 165, 177, 255}, {38, 40, 46, 255}},
	.Dock     = {{192, 133, 82, 255}, {48, 32, 18, 255}},
}

// As camadas, de baixo para cima.
Layer :: enum {
	Ocean,
	Islands,
	Marine_Life,
	Boats,
	Effects,
	Sky,
}

@(private = "file")
font: rl.Font
@(private = "file")
glyph_w: [256]f32 // largura de cada caractere, para centralizar na célula

render_init :: proc() {
	font = rl.GetFontDefault()
	scale := f32(FONT_SIZE) / f32(font.baseSize)
	for cp in 32 ..< 256 {
		info := rl.GetGlyphInfo(font, rune(cp))
		w := f32(info.advanceX) if info.advanceX > 0 else f32(info.image.width)
		glyph_w[cp] = w * scale
	}
}

// Desenha um caractere centralizado na célula (sx, sy) da tela.
glyph :: proc(sx, sy: i32, ch: rune, color: rl.Color) {
	w := glyph_w[ch] if ch < 256 else f32(CELL_W)
	pos := rl.Vector2{f32(sx * CELL_W) + (f32(CELL_W) - w) / 2, f32(sy * CELL_H)}
	rl.DrawTextCodepoint(font, ch, pos, FONT_SIZE, color)
}

text :: proc(str: string, x, y: f32, size: f32, color: rl.Color) {
	rl.DrawTextEx(font, cstr(str), {x, y}, size, size / 10, color)
}

text_width :: proc(str: string, size: f32) -> f32 {
	return rl.MeasureTextEx(font, cstr(str), size, size / 10).x
}

// Hash rápido para variar ondas, peixes e nuvens sem guardar estado.
@(private = "file")
hash2 :: proc(x, y: i32, salt: u32) -> u32 {
	return world_hash(x, y, salt)
}

@(private = "file")
View :: struct {
	cols, rows: i32,
	ox, oy:     i32, // célula do mundo no canto superior esquerdo
	t:          i32, // tempo em "ticks" de 100 ms, para animar
}

@(private = "file")
view :: proc() -> View {
	cols := (rl.GetScreenWidth() - PANEL_W) / CELL_W
	rows := rl.GetScreenHeight() / CELL_H
	return {cols, rows, s.x - cols / 2, s.y - rows / 2, i32(rl.GetTime() * 10)}
}

@(private = "file")
in_view :: proc(v: View, x, y: i32) -> (sx, sy: i32, ok: bool) {
	sx, sy = x - v.ox, y - v.oy
	return sx, sy, sx >= 0 && sy >= 0 && sx < v.cols && sy < v.rows
}

draw_world :: proc() {
	v := view()
	for layer in Layer {
		switch layer {
		case .Ocean, .Islands:
			draw_terrain(v, layer)
		case .Marine_Life:
			draw_schools(v)
		case .Boats:
			draw_boats(v)
		case .Effects:
			draw_effects(v)
		case .Sky:
			draw_sky(v)
		}
	}
}

@(private = "file")
tile_glyph :: proc(t: Tile, x, y, time: i32) -> rune {
	switch t {
	case .Deep, .Water:
		// Ondas que andam devagar para o mar parecer vivo.
		wave := (x + time / 4 + y / 2) %% 7
		return '-' if wave == 0 else '~'
	case .Shallow:
		return '~' if hash2(x, y, 7) % 100 < 15 else '.'
	case .Sand:
		return ':'
	case .Grass:
		return '"'
	case .Forest:
		return '%' if hash2(x, y, 3) % 2 == 0 else '&'
	case .Mountain:
		return '^'
	case .Dock:
		return 'H'
	}
	return ' '
}

@(private = "file")
draw_terrain :: proc(v: View, layer: Layer) {
	for sy in 0 ..< v.rows {
		for sx in 0 ..< v.cols {
			x, y := v.ox + sx, v.oy + sy
			if x < 0 || y < 0 || x >= MAP_W || y >= MAP_H do continue
			t := s.tiles[y * MAP_W + x]
			if tile_is_water(t) != (layer == .Ocean) do continue
			style := TILE_STYLE[t]
			rl.DrawRectangle(sx * CELL_W, sy * CELL_H, CELL_W, CELL_H, style.bg)
			glyph(sx, sy, tile_glyph(t, x, y, v.t), style.fg)
		}
	}
	if layer == .Islands {
		// NPC do porto.
		if sx, sy, ok := in_view(v, DOCK_X, DOCK_Y - 1); ok do glyph(sx, sy, 'L', COLOR_ACCENT)
	}
}

@(private = "file")
draw_schools :: proc(v: View) {
	for sc in s.schools[:s.school_count] {
		r := i32(sc.r)
		for y in sc.y - r ..= sc.y + r {
			for x in sc.x - r ..= sc.x + r {
				dx, dy := x - sc.x, y - sc.y
				if dx * dx + dy * dy > r * r || !tile_is_water(tile_at(x, y)) do continue
				sx, sy := in_view(v, x, y) or_continue
				// Peixinhos >< piscando dentro do raio.
				h := hash2(x + v.t / 3, y, u32(sc.x) * 31 + u32(sc.y)) % 100
				if h < 45 do glyph(sx, sy, '>' if h < 22 else '<', COLOR_SCHOOL)
			}
		}
	}
}

@(private = "file")
draw_label :: proc(v: View, x, y: i32, str: string, color: rl.Color) {
	sx, sy, ok := in_view(v, x, y)
	if !ok do return
	w := text_width(str, 10)
	text(str, f32(sx * CELL_W + CELL_W / 2) - w / 2, f32(sy * CELL_H + 4), 10, color)
}

@(private = "file")
draw_boats :: proc(v: View) {
	for b in s.boats[:s.boat_count] {
		p := s.players[b.id]
		color := PLAYER_COLORS[p.color % len(PLAYER_COLORS)]
		if sx, sy, ok := in_view(v, b.x, b.y); ok do glyph(sx, sy, '@', color)
		draw_label(v, b.x, b.y - 1, p.name, rl.WHITE)
	}
	sx, sy, _ := in_view(v, s.x, s.y)
	// Um brilho em volta do seu barco para achar ele fácil.
	pulse := u8(60 + 40 * math.sin(rl.GetTime() * 4))
	rl.DrawRectangle(sx * CELL_W, sy * CELL_H, CELL_W, CELL_H, {255, 209, 102, pulse})
	glyph(sx, sy, '@', COLOR_ACCENT)
}

@(private = "file")
draw_effects :: proc(v: View) {
	// Rastro do barco: os últimos lugares por onde passou, sumindo aos poucos.
	for k in 0 ..< len(s.wake) {
		w := s.wake[(s.wake_next + k) % len(s.wake)] // k = 0 é o mais antigo
		if w == {s.x, s.y} || w == {0, 0} do continue
		if sx, sy, ok := in_view(v, w[0], w[1]); ok do glyph(sx, sy, '°', {220, 240, 255, u8(40 + k * 18)})
	}
	// Peixe pescado / dinheiro subindo do barco.
	now := rl.GetTime()
	sx, sy, _ := in_view(v, s.x, s.y)
	for p in s.popups {
		age := now - p.time
		if p.time == 0 || age > 1.6 do continue
		alpha := u8(255 * (1 - age / 1.6))
		w := text_width(p.text, 20)
		x := f32(sx * CELL_W + CELL_W / 2) - w / 2
		y := f32(sy * CELL_H) - 24 - f32(age) * 40
		text(p.text, x, y, 20, {p.color.r, p.color.g, p.color.b, alpha})
	}
}

// Céu: nuvens passando e um ciclo de dia e noite (10 minutos).
// À noite, só o entorno do barco fica iluminado.
@(private = "file")
draw_sky :: proc(v: View) {
	day := math.sin(rl.GetTime() * math.TAU / 600)
	night := f32(clamp(-day * 1.4, 0, 1)) // 0 de dia, 1 no meio da noite
	drift := i32(rl.GetTime() * 2)
	for sy in 0 ..< v.rows {
		for sx in 0 ..< v.cols {
			x, y := v.ox + sx, v.oy + sy
			// Nuvem: ruído grosso que anda para o leste.
			c := cloud(x - drift, y)
			if c > 0 do rl.DrawRectangle(sx * CELL_W, sy * CELL_H, CELL_W, CELL_H, {230, 240, 255, u8(c * 70)})
			if night > 0 {
				dx, dy := f32(x - s.x), f32(y - s.y) * 1.6
				light := clamp(math.sqrt(dx * dx + dy * dy) / 14, 0, 1)
				a := u8(night * 170 * light)
				if a > 0 do rl.DrawRectangle(sx * CELL_W, sy * CELL_H, CELL_W, CELL_H, {2, 6, 24, a})
			}
		}
	}
}

@(private = "file")
cloud :: proc(x, y: i32) -> f32 {
	CELL :: 24
	ix, iy := floor_div(x, CELL), floor_div(y, CELL)
	fx := f32(x - ix * CELL) / CELL
	fy := f32(y - iy * CELL) / CELL
	a := f32(hash2(ix, iy, 99) % 1000) / 1000
	b := f32(hash2(ix + 1, iy, 99) % 1000) / 1000
	c := f32(hash2(ix, iy + 1, 99) % 1000) / 1000
	d := f32(hash2(ix + 1, iy + 1, 99) % 1000) / 1000
	fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
	n := math.lerp(math.lerp(a, b, fx), math.lerp(c, d, fx), fy)
	return clamp((n - 0.72) * 4, 0, 1)
}

@(private = "file")
floor_div :: proc(a, b: i32) -> i32 {
	q := a / b
	if (a % b != 0) && ((a < 0) != (b < 0)) do q -= 1
	return q
}
