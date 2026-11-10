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
COLOR_SHOT :: rl.Color{255, 150, 70, 255}
COLOR_BAG :: rl.Color{214, 160, 96, 255}
COLOR_MERMAID :: rl.Color{255, 159, 243, 255}
COLOR_VILLAGE :: rl.Color{150, 230, 140, 255}
COLOR_SHARK :: rl.Color{190, 200, 214, 255}
COLOR_GHOST :: rl.Color{150, 255, 190, 255}
COLOR_MEGALODON :: rl.Color{140, 170, 200, 255}
COLOR_KRAKEN :: rl.Color{220, 100, 170, 255}
COLOR_CTHULHU :: rl.Color{110, 230, 150, 255}

// Iguais ao game.c, só para desenhar.
KRAKEN_REACH :: 12 // alcance dos tentáculos
CTHULHU_MADNESS_R :: 15 // raio da loucura

// Desenho de cada boss, linha por linha, com o centro do corpo na linha do meio.
MEGALODON_ART := [3]string{`   _/|_   `, `<o======<<`, `   '\|'   `}
KRAKEN_ART := [4]string{` .-""-. `, `( o  o )`, ` )(||)( `, `/ /||\ \`}
CTHULHU_ART := [5]string{`  _____  `, ` ( o o ) `, ` /|||||\ `, `/ ||||| \`, `  /   \  `}

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

// Célula do mundo embaixo do mouse (ok = false com o mouse em cima do painel).
mouse_cell :: proc() -> (x, y: i32, ok: bool) {
	v := view()
	m := rl.GetMousePosition()
	if m.x < 0 || m.y < 0 do return 0, 0, false
	sx, sy := i32(m.x) / CELL_W, i32(m.y) / CELL_H
	if sx >= v.cols || sy >= v.rows do return 0, 0, false
	return v.ox + sx, v.oy + sy, true
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
			draw_predators(v)
			draw_boss(v)
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
		// NPCs do porto: mecânico (atrás), mercenário, lojista e estaleiro.
		if sx, sy, ok := in_view(v, DOCK_X, DOCK_Y - 2); ok do glyph(sx, sy, 'C', COLOR_ACCENT)
		if sx, sy, ok := in_view(v, DOCK_X - 2, DOCK_Y - 1); ok do glyph(sx, sy, 'M', COLOR_ACCENT)
		if sx, sy, ok := in_view(v, DOCK_X, DOCK_Y - 1); ok do glyph(sx, sy, 'L', COLOR_ACCENT)
		if sx, sy, ok := in_view(v, DOCK_X + 2, DOCK_Y - 1); ok do glyph(sx, sy, 'E', COLOR_ACCENT)
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
	// Bags ($) e coroas (W) boiando, piscando para chamar atenção, a sereia (S) na
	// pedra dela e o cais das vilas (V).
	for m in s.marks[:s.mark_count] {
		if m.kind == MARK_SCHOOL do continue
		sx, sy := in_view(v, m.x, m.y) or_continue
		bright := u8(180 + 75 * math.sin(rl.GetTime() * 5))
		switch m.kind {
		case MARK_BAG:
			glyph(sx, sy, '$', {COLOR_BAG.r, COLOR_BAG.g, COLOR_BAG.b, bright})
		case MARK_CROWN:
			glyph(sx, sy, 'W', {COLOR_ACCENT.r, COLOR_ACCENT.g, COLOR_ACCENT.b, bright})
		case MARK_MERMAID:
			glyph(sx, sy, 'S', COLOR_MERMAID)
			draw_label(v, m.x, m.y - 1, "Sereia", COLOR_MERMAID)
		case MARK_VILLAGE:
			glyph(sx, sy, 'V', COLOR_VILLAGE)
			draw_label(v, m.x, m.y - 1, "Vila", COLOR_VILLAGE)
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

// Tubarões (^, a barbatana) e o navio fantasma. A vida aparece embaixo
// quando já levaram tiro.
@(private = "file")
draw_predators :: proc(v: View) {
	for p in s.predators[:s.pred_count] {
		if p.kind == PRED_GHOST {
			// Meio transparente e pulsando devagar: é um fantasma.
			alpha := u8(170 + 70 * math.sin(rl.GetTime() * 2))
			draw_boat(v, p.x, p.y, BOAT_LEVELS - 1, BOAT_LEVELS - 1, {COLOR_GHOST.r, COLOR_GHOST.g, COLOR_GHOST.b, alpha})
			draw_label(v, p.x, p.y - 2, "Navio fantasma", COLOR_GHOST)
		} else {
			sx, sy := in_view(v, p.x, p.y) or_continue
			glyph(sx, sy, '^', COLOR_SHARK)
		}
		if p.hp_pct < 100 do draw_hp_bar(v, p.x, p.y, p.hp_pct)
	}
}

@(private = "file")
boss_art :: proc(kind: u8) -> []string {
	switch kind {
	case BOSS_MEGALODON: return MEGALODON_ART[:]
	case BOSS_KRAKEN: return KRAKEN_ART[:]
	case BOSS_CTHULHU: return CTHULHU_ART[:]
	}
	return nil
}

// O boss: o desenho, o nome em cima e, no Kraken, os tentáculos até os barcos
// ao alcance. Perto do Cthulhu o mar fica esverdeado: é a área da loucura.
// Atacando (investida, tentáculos, anel), o desenho fica vermelho.
@(private = "file")
draw_boss :: proc(v: View) {
	b := s.boss
	if b.kind == BOSS_NONE do return
	if b.kind == BOSS_CTHULHU {
		alpha := u8(28 + 14 * math.sin(rl.GetTime() * 3))
		for y in b.y - CTHULHU_MADNESS_R ..= b.y + CTHULHU_MADNESS_R {
			for x in b.x - CTHULHU_MADNESS_R ..= b.x + CTHULHU_MADNESS_R {
				dx, dy := x - b.x, y - b.y
				if dx * dx + dy * dy > CTHULHU_MADNESS_R * CTHULHU_MADNESS_R do continue
				sx, sy := in_view(v, x, y) or_continue
				rl.DrawRectangle(sx * CELL_W, sy * CELL_H, CELL_W, CELL_H, {COLOR_CTHULHU.r, COLOR_CTHULHU.g, COLOR_CTHULHU.b, alpha})
			}
		}
	}
	if b.kind == BOSS_KRAKEN && b.attack > 0 {
		draw_tentacle(v, b, s.x, s.y)
		for o in s.boats[:s.boat_count] do draw_tentacle(v, b, o.x, o.y)
	}
	color := boss_color(b.kind)
	if b.attack > 0 do color = {255, 107, 107, 255}
	art := boss_art(b.kind)
	top := b.y - i32(len(art) / 2)
	for line, i in art do draw_shape(v, b.x, top + i32(i), line, color)
	draw_label(v, b.x, top - 1, boss_name(b.kind), boss_color(b.kind))
}

// Um tentáculo do Kraken até (x, y), se estiver ao alcance. O servidor agarra
// só os 3 mais perto, mas aqui desenha em todos: no meio da luta dá no mesmo.
@(private = "file")
draw_tentacle :: proc(v: View, b: Boss, x, y: i32) {
	dx, dy := x - b.x, y - b.y
	if dx * dx + dy * dy > KRAKEN_REACH * KRAKEN_REACH do return
	steps := max(abs(dx), abs(dy))
	for i in 1 ..< steps {
		sx, sy := in_view(v, b.x + dx * i / steps, b.y + dy * i / steps) or_continue
		glyph(sx, sy, '~' if i % 2 == 0 else 's', COLOR_KRAKEN)
	}
}

// Barrinha de vida embaixo de um barco ou de um bicho.
@(private = "file")
draw_hp_bar :: proc(v: View, x, y: i32, pct: u8) {
	sx, sy, ok := in_view(v, x, y)
	if !ok do return
	bx, by := f32((sx - 1) * CELL_W), f32((sy + 1) * CELL_H + 2)
	bw := f32(3 * CELL_W)
	rl.DrawRectangleV({bx, by}, {bw, 4}, {40, 50, 66, 255})
	rl.DrawRectangleV({bx, by}, {bw * f32(pct) / 100, 4}, {255, 107, 107, 255})
}

HULL_SHAPES := [BOAT_LEVELS]string{"@", "(@)", "\\@/", "\\=@=/", "<\\=@=/>"}
SAIL_SHAPES := [BOAT_LEVELS]string{"", "|", "/|", "/|\\", "/#\\"}

// Escreve `shape` na linha y, centralizado na coluna x.
@(private = "file")
draw_shape :: proc(v: View, x, y: i32, shape: string, color: rl.Color) {
	left := x - i32(len(shape) / 2)
	for ch, i in shape {
		if sx, sy, ok := in_view(v, left + i32(i), y); ok do glyph(sx, sy, ch, color)
	}
}

@(private = "file")
draw_boat :: proc(v: View, x, y: i32, hull, sail: int, color: rl.Color) {
	draw_shape(v, x, y, HULL_SHAPES[clamp(hull, 0, BOAT_LEVELS - 1)], color)
	draw_shape(v, x, y - 1, SAIL_SHAPES[clamp(sail, 0, BOAT_LEVELS - 1)], color)
}

@(private = "file")
draw_boats :: proc(v: View) {
	for b in s.boats[:s.boat_count] {
		p := s.players[b.id]
		color := PLAYER_COLORS[p.color % len(PLAYER_COLORS)]
		hull, sail := int(b.look >> 4), int(b.look & 0xF)
		draw_boat(v, b.x, b.y, hull, sail, color)
		label_y := b.y - 1
		if sail > 0 do label_y -= 1
		draw_label(v, b.x, label_y, p.name, rl.WHITE)
		// Barrinha de vida embaixo do barco, só quando já levou dano.
		if b.hp_pct < 100 do draw_hp_bar(v, b.x, b.y, b.hp_pct)
	}
	sx, sy, _ := in_view(v, s.x, s.y)
	pulse := u8(60 + 40 * math.sin(rl.GetTime() * 4))
	rl.DrawRectangle(sx * CELL_W, sy * CELL_H, CELL_W, CELL_H, {255, 209, 102, pulse})
	draw_boat(v, s.x, s.y, s.boat_level[PART_HULL], s.boat_level[PART_SAIL], COLOR_ACCENT)
}

@(private = "file")
draw_effects :: proc(v: View) {
	// Tiros voando.
	for shot in s.shots[:s.shot_count] {
		if sx, sy, ok := in_view(v, shot[0], shot[1]); ok do glyph(sx, sy, '*', COLOR_SHOT)
	}
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
	// Mira: um + embaixo do mouse, vermelho quando o alvo está no alcance.
	if wp, _, armed := equipped_weapon(); armed && !menu_open() {
		if mx, my, ok := mouse_cell(); ok {
			dx, dy := mx - s.x, my - s.y
			in_range := dx * dx + dy * dy <= i32(wp.range * wp.range)
			color := rl.Color{255, 90, 90, 255} if in_range && !s.safe else COLOR_QUIET
			if msx, msy, vis := in_view(v, mx, my); vis do glyph(msx, msy, '+', color)
		}
	}
}

// Céu: nuvens passando e um ciclo de dia e noite (10 minutos).
// À noite, só o entorno do barco fica iluminado. Na tempestade, céu escuro,
// chuva caindo com o vento e um relâmpago de vez em quando.
@(private = "file")
draw_sky :: proc(v: View) {
	day := math.sin(rl.GetTime() * math.TAU / 600)
	night := f32(clamp(-day * 1.4, 0, 1)) // 0 de dia, 1 no meio da noite
	drift := i32(rl.GetTime() * 2)
	storm := s.world.kind == WORLD_STORM
	flash := storm && hash2(i32(rl.GetTime() * 4), 0, 77) % 40 == 0 // relâmpago: ~1 a cada 10 s
	rain: rune = '/' if s.world.dx > 0 else ('\\' if s.world.dx < 0 else '|')
	for sy in 0 ..< v.rows {
		for sx in 0 ..< v.cols {
			x, y := v.ox + sx, v.oy + sy
			if storm {
				ex, ey := x - s.world.x, y - s.world.y
				if ex * ex + ey * ey <= s.world.radius * s.world.radius {
					sky := rl.Color{230, 240, 255, 90} if flash else rl.Color{14, 18, 30, 120}
					rl.DrawRectangle(sx * CELL_W, sy * CELL_H, CELL_W, CELL_H, sky)
					// A gota que está em (x, y) agora estava uma célula acima (e contra o vento) no tick anterior.
					if hash2(x - v.t * s.world.dx, y - v.t, 5) % 100 < 14 do glyph(sx, sy, rain, {170, 190, 225, 170})
				}
			}
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
