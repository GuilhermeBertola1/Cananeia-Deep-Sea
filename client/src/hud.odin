package game

// Interface por cima do mundo: painel lateral, minimapa, diário de bordo,
// loja do lojista e a tela de entrada.

import "core:fmt"
import "core:math"
import "core:strings"
import rl "vendor:raylib"

MINIMAP_SIZE :: 256 // metade do mapa: cada pixel = 2x2 células

cstr :: proc(str: string) -> cstring {
	return strings.clone_to_cstring(str, context.temp_allocator)
}

// --- minimapa --------------------------------------------------------------------

minimap_color :: proc(t: Tile) -> rl.Color {
	#partial switch t {
	case .Deep: return {10, 26, 56, 255}
	case .Water: return {24, 66, 122, 255}
	case .Shallow: return {56, 136, 168, 255}
	}
	return TILE_STYLE[t].fg
}

// Desenha o mapa inteiro numa textura uma vez só (gerada aqui, não é arquivo).
minimap_build :: proc() {
	pixels := make([]rl.Color, MAP_W * MAP_H, context.temp_allocator)
	for t, i in s.tiles do pixels[i] = minimap_color(t)
	img := rl.Image {
		data    = raw_data(pixels),
		width   = MAP_W,
		height  = MAP_H,
		mipmaps = 1,
		format  = .UNCOMPRESSED_R8G8B8A8,
	}
	if s.minimap.id != 0 do rl.UnloadTexture(s.minimap)
	s.minimap = rl.LoadTextureFromImage(img)
}

@(private = "file")
draw_map_at :: proc(x, y, size: f32) {
	rl.DrawTexturePro(s.minimap, {0, 0, MAP_W, MAP_H}, {x, y, size, size}, {}, 0, rl.WHITE)
	k := size / MAP_W
	dot :: proc(x, y, k: f32, cx, cy: i32, r: f32, color: rl.Color) {
		rl.DrawRectangleV({x + f32(cx) * k - r, y + f32(cy) * k - r}, {r * 2, r * 2}, color)
	}
	for m in s.marks[:s.mark_count] {
		if m.kind == MARK_SCHOOL do dot(x, y, k, m.x, m.y, max(1, k), COLOR_SCHOOL)
	}
	dot(x, y, k, PORT_X, PORT_Y, max(2, 2 * k), COLOR_ACCENT)
	for b in s.boats[:s.boat_count] {
		dot(x, y, k, b.x, b.y, max(2, 1.5 * k), PLAYER_COLORS[s.players[b.id].color % len(PLAYER_COLORS)])
	}
	if int(rl.GetTime() * 3) % 2 == 0 do dot(x, y, k, s.x, s.y, max(2, 2 * k), rl.WHITE)
}

// Cardume mais perto, para o jogador saber para onde ir.
@(private = "file")
nearest_target :: proc() -> (string, bool) {
	best := -1
	best_d2: i32 = max(i32)
	for m, i in s.marks[:s.mark_count] {
		dx, dy := m.x - s.x, m.y - s.y
		if d2 := dx * dx + dy * dy; d2 < best_d2 {best, best_d2 = i, d2}
	}
	if best < 0 do return "", false
	m := s.marks[best]
	DIRS := [8]string{"L", "SE", "S", "SO", "O", "NO", "N", "NE"}
	angle := math.atan2(f32(m.y - s.y), f32(m.x - s.x))
	dir := DIRS[(int(math.round(angle / (math.PI / 4))) + 8) % 8]
	dist := int(math.sqrt(f32(best_d2)))
	return fmt.tprintf("Cardume de %s: %d %s", species_name(int(m.species)), dist, dir), true
}

// --- painel ------------------------------------------------------------------------

draw_hud :: proc() {
	sw, sh := f32(rl.GetScreenWidth()), f32(rl.GetScreenHeight())
	px := sw - PANEL_W
	rl.DrawRectangleV({px, 0}, {PANEL_W, sh}, COLOR_PANEL)
	x := px + 14
	y: f32 = 12

	text("CANANEIA DEEP SEA", x, y, 20, COLOR_ACCENT)
	y += 30
	text(s.players[s.my_id].name, x, y, 20, COLOR_INK)
	money := money_text(s.money)
	text(money, sw - 14 - text_width(money, 20), y, 20, COLOR_ACCENT)
	y += 24
	text(fmt.tprintf("Porão %d/%d", hold_count(), s.capacity), x, y, 20, COLOR_INK)
	online := fmt.tprintf("%d online", s.online) if !s.offline else "offline"
	text(online, sw - 14 - text_width(online, 20), y, 20, COLOR_QUIET)
	y += 32

	draw_map_at(px + (PANEL_W - MINIMAP_SIZE) / 2, y, MINIMAP_SIZE)
	y += MINIMAP_SIZE + 6
	if target, ok := nearest_target(); ok {
		text(target, x, y, 10, COLOR_ACCENT)
		y += 14
	}
	text("M amplia o mapa", x, y, 10, COLOR_QUIET)
	y += 22

	text("RANKING", x, y, 20, COLOR_QUIET)
	y += 24
	for e, i in s.rank[:s.rank_count] {
		if y > sh - 40 do break
		color := COLOR_ACCENT if e.name == s.players[s.my_id].name else (COLOR_INK if e.online else COLOR_QUIET)
		text(fmt.tprintf("%d. %s", i + 1, e.name), x, y, 20, color)
		m := money_text(e.money)
		text(m, sw - 14 - text_width(m, 20), y, 20, color)
		y += 22
	}
	text("WASD/setas navega  E lojista  Esc fecha", x, sh - 18, 10, COLOR_QUIET)

	draw_log(px)
	if s.at_dock && !s.shop_open {
		tip := "Você está no cais. Aperte E para falar com o lojista."
		w := text_width(tip, 20)
		tx := (px - w) / 2
		rl.DrawRectangleV({tx - 10, 8}, {w + 20, 30}, {0, 0, 0, 170})
		text(tip, tx, 13, 20, COLOR_ACCENT)
	}
}

// Diário de bordo no canto de baixo, sumindo com o tempo.
@(private = "file")
draw_log :: proc(world_w: f32) {
	sh := f32(rl.GetScreenHeight())
	now := rl.GetTime()
	y := sh - 10 - f32(s.log_count) * 22
	for line, i in s.log[:s.log_count] {
		age := now - line.time
		alpha := u8(255 * clamp(1 - (age - 8) / 4, 0.25, 1)) // depois de 8 s vai apagando
		w := text_width(line.text, 20)
		rl.DrawRectangleV({6, y - 2}, {min(w + 12, world_w - 12), 22}, {0, 0, 0, alpha / 2})
		color := COLOR_INK if i == s.log_count - 1 else COLOR_QUIET
		text(line.text, 12, y, 20, {color.r, color.g, color.b, alpha})
		y += 22
	}
}

// --- loja ----------------------------------------------------------------------------

draw_shop :: proc() {
	world_w := f32(rl.GetScreenWidth() - PANEL_W)
	w: f32 = 620
	rows := 0
	for h in s.hold do rows += 1 if h > 0 else 0
	h := f32(170 + max(rows, 1) * 26)
	x := (world_w - w) / 2
	y := (f32(rl.GetScreenHeight()) - h) / 2
	rl.DrawRectangleV({x, y}, {w, h}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({x, y, w, h}, 2, COLOR_ACCENT)

	cx := x + 20
	cy := y + 16
	text("LOJISTA", cx, cy, 20, COLOR_ACCENT)
	cy += 26
	text("\"Peixe fresco? Pago o preço do dia, marujo.\"", cx, cy, 20, COLOR_QUIET)
	cy += 36

	total: u32
	if rows == 0 {
		text("Porão vazio. Vá pescar!", cx, cy, 20, COLOR_INK)
		cy += 26
	}
	for n, i in s.hold {
		if n == 0 do continue
		price := u32(s.prices[i])
		value := price * u32(n)
		total += value
		text(fmt.tprintf("[%d] %s", i + 1, species_name(i)), cx, cy, 20, SPECIES[i].color)
		text(fmt.tprintf("x%d", n), cx + 280, cy, 20, COLOR_INK)
		text(money_text(price), cx + 350, cy, 20, COLOR_QUIET)
		v := money_text(value)
		text(v, x + w - 20 - text_width(v, 20), cy, 20, COLOR_INK)
		cy += 26
	}
	cy += 10
	text(fmt.tprintf("[T] Vender tudo por %s", money_text(total)), cx, cy, 20, COLOR_ACCENT)
	cy += 30
	text("Número vende a espécie.  E ou Esc fecha.", cx, cy, 20, COLOR_QUIET)
}

draw_big_map :: proc() {
	sw, sh := f32(rl.GetScreenWidth()), f32(rl.GetScreenHeight())
	rl.DrawRectangleV({0, 0}, {sw, sh}, {0, 0, 0, 160})
	size := math.floor(min(sw, sh) * 0.86 / MAP_W * 4) / 4 * MAP_W // múltiplo de 1/4 para os pixels ficarem iguais
	x, y := (sw - size) / 2, (sh - size) / 2
	draw_map_at(x, y, size)
	rl.DrawRectangleLinesEx({x - 2, y - 2, size + 4, size + 4}, 2, COLOR_ACCENT)
	text("Mapa do oceano  -  M ou Esc fecha", x, y - 26, 20, COLOR_ACCENT)
}

// --- tela de entrada ---------------------------------------------------------------

LOGO := [?]string {
	`        |\`,
	`        | \         C A N A N E I A`,
	`        |__\          D E E P   S E A`,
	`     ___|___\___`,
	`~~~~ \_________/ ~~~~~~~~~~~~~~~~~~~~~~~~~~`,
}

draw_join :: proc() {
	sw, sh := rl.GetScreenWidth(), rl.GetScreenHeight()
	cols := sw / CELL_W
	top := sh / CELL_H / 2 - 7
	left := (cols - 44) / 2
	for line, row in LOGO {
		for ch, col in line {
			color := TILE_STYLE[.Water].fg if ch == '~' else COLOR_ACCENT
			glyph(left + i32(col), top + i32(row), ch, color)
		}
	}

	y := f32((top + 7) * CELL_H)
	bx := f32(sw) / 2 - 170
	text("Nome do capitão:", bx, y, 20, COLOR_QUIET)
	y += 28
	rl.DrawRectangleV({bx, y}, {340, 34}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({bx, y, 340, 34}, 2, COLOR_ACCENT)
	name := string(s.name_buf[:s.name_len])
	text(name, bx + 10, y + 7, 20, COLOR_INK)
	if int(rl.GetTime() * 2) % 2 == 0 {
		cx := bx + 12 + text_width(name, 20)
		rl.DrawRectangleV({cx, y + 7}, {10, 20}, COLOR_ACCENT)
	}
	y += 48
	text("Enter para zarpar", bx, y, 20, COLOR_ACCENT)
	y += 34
	if s.join_message != "" do text(s.join_message, bx, y, 20, {255, 140, 120, 255})

	server := fmt.tprintf("Servidor: %s", s.url) if s.url != "" else "Sem servidor (versão desktop): modo offline"
	text(server, 12, f32(sh) - 22, 10, COLOR_QUIET)
}
