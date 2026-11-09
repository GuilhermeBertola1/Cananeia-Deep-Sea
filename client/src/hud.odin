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
		switch m.kind {
		case MARK_SCHOOL: dot(x, y, k, m.x, m.y, max(1, k), COLOR_SCHOOL)
		case MARK_BAG: dot(x, y, k, m.x, m.y, max(2, 1.5 * k), COLOR_BAG)
		case MARK_CROWN: dot(x, y, k, m.x, m.y, max(2, 1.5 * k), COLOR_ACCENT)
		case MARK_MERMAID: dot(x, y, k, m.x, m.y, max(2, 2 * k), COLOR_MERMAID)
		}
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
		if m.kind != MARK_SCHOOL do continue // bags e coroas também são marcas
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
	y += 24

	// Vida do barco: verde, amarela abaixo da metade, vermelha no fim.
	life := "AVARIADO" if s.broken else fmt.tprintf("Vida %d/%d", s.hp, s.hp_max)
	text(life, x, y, 20, COLOR_INK)
	vel := fmt.tprintf("Vel %d/%d", s.vel, s.speed)
	text(vel, sw - 14 - text_width(vel, 20), y, 20, COLOR_QUIET)
	y += 22
	frac := f32(s.hp) / f32(max(s.hp_max, 1))
	bar_w: f32 = PANEL_W - 28
	bar_color := rl.Color{128, 237, 153, 255} if frac > 0.5 else (COLOR_ACCENT if frac > 0.25 else rl.Color{255, 107, 107, 255})
	rl.DrawRectangleV({x, y}, {bar_w, 8}, {40, 50, 66, 255})
	rl.DrawRectangleV({x, y}, {bar_w * frac, 8}, bar_color)
	y += 14

	// Arma: pronta ou recarregando. Na zona segura, o aviso no lugar.
	if s.safe {
		text("Zona segura: ninguém atira aqui", x, y, 10, {128, 237, 153, 255})
	} else if _, base, armed := equipped_weapon(); armed {
		state := "pronta" if s.reload == 0 else "recarregando..."
		text(fmt.tprintf("%s: %s", item_name(base), state), x, y, 10, COLOR_INK if s.reload == 0 else COLOR_QUIET)
	} else {
		text("Sem arma: equipe uma no inventário (I)", x, y, 10, COLOR_QUIET)
	}
	y += 16
	if s.has_mission {
		text(fmt.tprintf("Missão da sereia: afundar %s", mission_target()), x, y, 10, COLOR_MERMAID)
		y += 14
	}

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
		if y > sh - 54 do break
		color := COLOR_ACCENT if e.name == s.players[s.my_id].name else (COLOR_INK if e.online else COLOR_QUIET)
		text(fmt.tprintf("%d. %s", i + 1, e.name), x, y, 20, color)
		m := money_text(e.money)
		text(m, sw - 14 - text_width(m, 20), y, 20, color)
		y += 22
	}
	text("B mercenário  C mecânico  E sereia (S, no mar)", x, sh - 46, 10, COLOR_QUIET)
	text("Clique ou Espaço atira na mira (+)", x, sh - 32, 10, COLOR_QUIET)
	text("WASD/setas navega  E lojista  U estaleiro  I inventário  Esc fecha", x, sh - 18, 10, COLOR_QUIET)

	draw_log(px)
	tip := ""
	if s.at_dock do tip = "No cais. E: lojista (L)  U: estaleiro (E)  B: mercenário (M)  C: mecânico (C)"
	if s.at_mermaid do tip = "A sereia (S) está ouvindo. E: falar com ela"
	if tip != "" && !menu_open() {
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

// --- estaleiro --------------------------------------------------------------------

draw_shipyard :: proc() {
	world_w := f32(rl.GetScreenWidth() - PANEL_W)
	w: f32 = 700
	h: f32 = 330
	x := (world_w - w) / 2
	y := (f32(rl.GetScreenHeight()) - h) / 2
	rl.DrawRectangleV({x, y}, {w, h}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({x, y, w, h}, 2, COLOR_ACCENT)

	cx := x + 20
	cy := y + 16
	text("ESTALEIRO", cx, cy, 20, COLOR_ACCENT)
	cy += 26
	text("\"Barco bom é barco cuidado. O que vai ser hoje?\"", cx, cy, 20, COLOR_QUIET)
	cy += 40

	// Cada peça: nível agora, efeito agora -> efeito do próximo nível e o preço.
	for part in 0 ..< PART_COUNT {
		lv := s.boat_level[part]
		text(fmt.tprintf("[%d] %s  nível %d", part + 1, PART_NAMES[part], lv + 1), cx, cy, 20, COLOR_INK)
		if lv + 1 < BOAT_LEVELS {
			price := UPGRADE_PRICE[lv + 1]
			price_text := money_text(price)
			text(price_text, x + w - 20 - text_width(price_text, 20), cy, 20, COLOR_ACCENT if s.money >= price else COLOR_QUIET)
			text(fmt.tprintf("%s  ->  %s", part_effect(part, lv), part_effect(part, lv + 1)), cx + 36, cy + 24, 20, COLOR_QUIET)
		} else {
			text("máximo", x + w - 20 - text_width("máximo", 20), cy, 20, COLOR_QUIET)
			text(part_effect(part, lv), cx + 36, cy + 24, 20, COLOR_QUIET)
		}
		cy += 56
	}

	cy += 6
	missing := s.hp_max - s.hp
	if missing > 0 {
		cost := money_text(u32(missing * REPAIR_CENTS_PER_HP))
		text(fmt.tprintf("[R] Consertar %d de vida por %s", missing, cost), cx, cy, 20, COLOR_ACCENT)
	} else {
		text("Casco inteiro, nada para consertar.", cx, cy, 20, COLOR_QUIET)
	}
	cy += 34
	text("Número compra o próximo nível.  U ou Esc fecha.", cx, cy, 20, COLOR_QUIET)
}

// --- mercenário -----------------------------------------------------------------

draw_mercenary :: proc() {
	world_w := f32(rl.GetScreenWidth() - PANEL_W)
	w: f32 = 680
	h: f32 = 180 + LOOT_SLOTS * 26
	x := (world_w - w) / 2
	y := (f32(rl.GetScreenHeight()) - h) / 2
	rl.DrawRectangleV({x, y}, {w, h}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({x, y, w, h}, 2, COLOR_ACCENT)

	cx := x + 20
	cy := y + 16
	text("MERCENÁRIO", cx, cy, 20, COLOR_ACCENT)
	cy += 26
	text("\"Bag dos outros? Eu fico com ela, os peixes ficam com você.\"", cx, cy, 20, COLOR_QUIET)
	cy += 40

	// O que o barco carrega. A coroa do alvo da sereia ele não pega.
	fish, crowns := 0, 0
	for &l in s.loot {
		switch l.kind {
		case LOOT_BAG:
			text(fmt.tprintf("$ Bag de %s", loot_owner(&l)), cx, cy, 20, COLOR_BAG)
			n := fmt.tprintf("%d peixes", l.fish)
			text(n, x + w - 20 - text_width(n, 20), cy, 20, COLOR_INK)
			fish += l.fish
		case LOOT_CROWN:
			if s.has_mission && loot_owner(&l) == mission_target() {
				text(fmt.tprintf("W Coroa de %s (essa é da sereia)", loot_owner(&l)), cx, cy, 20, COLOR_MERMAID)
			} else {
				text(fmt.tprintf("W Coroa de %s", loot_owner(&l)), cx, cy, 20, COLOR_ACCENT)
				bounty := money_text(CROWN_BOUNTY_CENTS)
				text(bounty, x + w - 20 - text_width(bounty, 20), cy, 20, COLOR_INK)
				crowns += 1
			}
		case:
			continue
		}
		cy += 26
	}
	if fish == 0 && crowns == 0 do text("Nada para entregar. Afunde alguém e pegue a bag!", cx, cy, 20, COLOR_QUIET)

	// Os peixes vão para o porão: só cabe o que tem de espaço.
	room := max(s.capacity - hold_count(), 0)
	cy = y + h - 64
	deal := fmt.tprintf("[T] Entregar: %d peixe(s) para o porão e %s", min(fish, room), money_text(u32(crowns) * CROWN_BOUNTY_CENTS))
	text(deal, cx, cy, 20, COLOR_ACCENT if fish > 0 || crowns > 0 else COLOR_QUIET)
	cy += 30
	text("T entrega.  B ou Esc fecha.", cx, cy, 20, COLOR_QUIET)
}

// --- sereia -------------------------------------------------------------------------

draw_mermaid :: proc() {
	world_w := f32(rl.GetScreenWidth() - PANEL_W)
	w: f32 = 700
	h: f32 = 260
	x := (world_w - w) / 2
	y := (f32(rl.GetScreenHeight()) - h) / 2
	rl.DrawRectangleV({x, y}, {w, h}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({x, y, w, h}, 2, COLOR_MERMAID)

	cx := x + 20
	cy := y + 16
	text("SEREIA", cx, cy, 20, COLOR_MERMAID)
	cy += 26
	if !s.has_mission {
		text("\"Gire a roleta, marujo. A sorte é do mar...\"", cx, cy, 20, COLOR_QUIET)
		cy += 40
		text("40%  ela rouba 10% do seu dinheiro (no máximo $100)", cx, cy, 20, COLOR_INK)
		cy += 26
		text("45%  missão: afundar um capitão e trazer a coroa dele", cx, cy, 20, COLOR_INK)
		cy += 26
		text("15%  ela só canta", cx, cy, 20, COLOR_INK)
		cy += 40
		text("[G] Girar a roleta (uma vez por minuto)", cx, cy, 20, COLOR_MERMAID)
	} else {
		target := mission_target()
		text(fmt.tprintf("\"Afunde %s e me traga a coroa. Eu encanto um item seu.\"", target), cx, cy, 20, COLOR_QUIET)
		cy += 40
		has_crown := false
		for &l in s.loot {
			if l.kind == LOOT_CROWN && loot_owner(&l) == target do has_crown = true
		}
		if has_crown {
			text(fmt.tprintf("[G] Entregar a coroa de %s", target), cx, cy, 20, COLOR_MERMAID)
			cy += 26
			text("Ela encanta um item aleatório da bolsa (até +3).", cx, cy, 20, COLOR_QUIET)
		} else {
			text(fmt.tprintf("Você ainda não tem a coroa de %s.", target), cx, cy, 20, COLOR_INK)
			cy += 26
			text("Afunde o barco e passe por cima da coroa (W) no mar.", cx, cy, 20, COLOR_QUIET)
		}
		cy += 40
		text("[X] Desistir da missão", cx, cy, 20, COLOR_QUIET)
	}
	text("E ou Esc fecha.", cx, y + h - 30, 20, COLOR_QUIET)
}

// --- mecânico ------------------------------------------------------------------

draw_mechanic :: proc() {
	world_w := f32(rl.GetScreenWidth() - PANEL_W)
	w: f32 = 640
	h: f32 = 170 + BAG_SIZE * 26
	x := (world_w - w) / 2
	y := (f32(rl.GetScreenHeight()) - h) / 2
	rl.DrawRectangleV({x, y}, {w, h}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({x, y, w, h}, 2, COLOR_ACCENT)

	cx := x + 20
	cy := y + 16
	text("MECÂNICO", cx, cy, 20, COLOR_ACCENT)
	cy += 26
	text("\"Dois iguais viram um melhor. Pode confiar.\"", cx, cy, 20, COLOR_QUIET)
	cy += 36

	// A bolsa. > = o primeiro item escolhido.
	for it, i in s.bag {
		if i == s.mech_cursor do rl.DrawRectangleV({cx - 8, cy - 3}, {w - 24, 26}, {255, 255, 255, 28})
		if it.base == ITEM_NONE {
			text(fmt.tprintf("%2d   -", i + 1), cx, cy, 20, COLOR_QUIET)
		} else {
			mark := ">" if s.mech_first == i + 1 else " "
			ench := fmt.tprintf(" +%d", it.enchant) if it.enchant > 0 else ""
			text(fmt.tprintf("%2d %s %s%s (%s)", i + 1, mark, item_name(it.base), ench, rarity_name(it.rarity)), cx, cy, 20, rarity_color(it.rarity))
		}
		cy += 26
	}

	// O que sai se juntar o marcado com o que está no cursor.
	cy += 8
	if s.mech_first == 0 {
		text("Enter marca o primeiro item.", cx, cy, 20, COLOR_INK)
	} else {
		a, b := s.bag[s.mech_first - 1], s.bag[s.mech_cursor]
		if s.mech_first == s.mech_cursor + 1 || b.base == ITEM_NONE {
			text(fmt.tprintf("Escolha outro %s para juntar.", item_name(a.base)), cx, cy, 20, COLOR_INK)
		} else if a.base != b.base {
			text("Os dois precisam ser do mesmo tipo.", cx, cy, 20, {255, 140, 120, 255})
		} else {
			r := combine_rarity(a, b)
			price := COMBINE_PRICE[r]
			color := COLOR_ACCENT if s.money >= price else COLOR_QUIET
			text(fmt.tprintf("[Enter] Juntar: sai %s ou melhor, por %s", rarity_name(r), money_text(price)), cx, cy, 20, color)
		}
	}
	text("W/S escolhe  Enter marca/junta  C ou Esc fecha", cx, y + h - 30, 20, COLOR_QUIET)
}

// --- inventário -----------------------------------------------------------------

draw_inventory :: proc() {
	world_w := f32(rl.GetScreenWidth() - PANEL_W)
	w: f32 = 760
	h: f32 = 100 + BAG_SIZE * 26 + 36 + LOOT_SLOTS * 24
	x := (world_w - w) / 2
	y := (f32(rl.GetScreenHeight()) - h) / 2
	rl.DrawRectangleV({x, y}, {w, h}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({x, y, w, h}, 2, COLOR_ACCENT)

	cx := x + 20
	text("INVENTÁRIO", cx, y + 16, 20, COLOR_ACCENT)

	// Lista da bolsa à esquerda. * = equipado.
	list_w: f32 = 400
	row_y := y + 52
	for it, i in s.bag {
		if i == s.inv_cursor do rl.DrawRectangleV({cx - 8, row_y - 3}, {list_w, 26}, {255, 255, 255, 28})
		if it.base == ITEM_NONE {
			text(fmt.tprintf("%2d   -", i + 1), cx, row_y, 20, COLOR_QUIET)
		} else {
			mark := "*" if it.equipped != 0 else " "
			ench := fmt.tprintf(" +%d", it.enchant) if it.enchant > 0 else ""
			text(fmt.tprintf("%2d %s %s%s", i + 1, mark, item_name(it.base), ench), cx, row_y, 20, rarity_color(it.rarity))
			if it.equipped != 0 {
				slot := EQUIP_NAMES[it.equipped] if int(it.equipped) < len(EQUIP_NAMES) else "?"
				text(slot, cx + list_w - 20 - text_width(slot, 20), row_y, 20, COLOR_ACCENT)
			}
		}
		row_y += 26
	}

	// Saque que o barco carrega (bags e coroas pegas no mar) e o placar.
	row_y += 10
	text("SAQUE", cx, row_y, 20, COLOR_QUIET)
	score := fmt.tprintf("abates %d  mortes %d", s.kills, s.deaths)
	text(score, cx + list_w - 20 - text_width(score, 20), row_y, 20, COLOR_QUIET)
	row_y += 26
	empty := true
	for &l in s.loot {
		switch l.kind {
		case LOOT_BAG: text(fmt.tprintf("$ Bag de %s (%d peixes)", loot_owner(&l), l.fish), cx, row_y, 20, COLOR_BAG)
		case LOOT_CROWN:
			// A coroa que a sereia pediu fica rosa.
			mission := s.has_mission && loot_owner(&l) == mission_target()
			text(fmt.tprintf("W Coroa de %s", loot_owner(&l)), cx, row_y, 20, COLOR_MERMAID if mission else COLOR_ACCENT)
		case: continue
		}
		empty = false
		row_y += 24
	}
	if empty do text("Nada por enquanto.", cx, row_y, 20, COLOR_QUIET)

	// Detalhes do item escolhido à direita.
	dx := cx + list_w + 20
	dy := y + 52
	sel := s.bag[s.inv_cursor]
	if sel.base == ITEM_NONE {
		text("Slot vazio", dx, dy, 20, COLOR_QUIET)
	} else {
		text(item_name(sel.base), dx, dy, 20, rarity_color(sel.rarity))
		dy += 24
		text(rarity_name(sel.rarity), dx, dy, 20, rarity_color(sel.rarity))
		if sel.enchant > 0 {
			dy += 24
			text(fmt.tprintf("Encanto da sereia +%d", sel.enchant), dx, dy, 20, COLOR_MERMAID)
		}
		dy += 32
		for v, st in sel.stats {
			if v == 0 do continue
			text(fmt.tprintf("%+d%s %s", v, STAT_UNITS[st], STAT_NAMES[st]), dx, dy, 20, COLOR_INK)
			dy += 24
		}
		for a in sel.affix {
			if a == AFFIX_NONE do continue
			text(fmt.tprintf("[%s]", affix_name(a)), dx, dy, 20, COLOR_QUIET)
			dy += 24
		}
	}

	// Total do barco embaixo, à direita.
	dy = y + h - 44 - STAT_COUNT * 24 - 28
	text("SEU BARCO", dx, dy, 20, COLOR_QUIET)
	dy += 28
	for v, st in s.stats {
		text(fmt.tprintf("%s: %d%s", STAT_NAMES[st], v, STAT_UNITS[st]), dx, dy, 20, COLOR_INK)
		dy += 24
	}

	if s.discard_armed {
		text("Aperte X de novo para jogar fora. Não tem volta!", cx, y + h - 30, 20, {255, 140, 120, 255})
	} else {
		text("W/S escolhe  Enter equipa/tira  X joga fora  I fecha", cx, y + h - 30, 20, COLOR_QUIET)
	}
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

@(private = "file")
draw_input :: proc(label, value: string, x, y: f32, focused: bool) -> f32 {
	text(label, x, y, 20, COLOR_QUIET)
	box_y := y + 28
	rl.DrawRectangleV({x, box_y}, {340, 34}, COLOR_PANEL)
	rl.DrawRectangleLinesEx({x, box_y, 340, 34}, 2, COLOR_ACCENT if focused else COLOR_QUIET)
	text(value, x + 10, box_y + 7, 20, COLOR_INK)
	if focused && int(rl.GetTime() * 2) % 2 == 0 {
		cx := x + 12 + text_width(value, 20)
		rl.DrawRectangleV({cx, box_y + 7}, {10, 20}, COLOR_ACCENT)
	}
	return box_y + 48
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
	y = draw_input("Nome do capitão:", string(s.name_buf[:s.name_len]), bx, y, !s.typing_pass)
	if s.url != "" {
		stars := strings.repeat("*", s.pass_len, context.temp_allocator)
		y = draw_input("Senha (nome novo cria a conta):", stars, bx, y, s.typing_pass)
		text("Enter para zarpar  -  Tab troca de campo", bx, y, 20, COLOR_ACCENT)
	} else {
		text("Enter para zarpar", bx, y, 20, COLOR_ACCENT)
	}
	y += 34
	if s.join_message != "" do text(s.join_message, bx, y, 20, {255, 140, 120, 255})

	server := fmt.tprintf("Servidor: %s", s.url) if s.url != "" else "Sem servidor (versão desktop): modo offline"
	text(server, 12, f32(sh) - 22, 10, COLOR_QUIET)
}
