package game

// Estado do cliente e o laço de cada quadro. O cliente não decide nada:
// manda a direção e os pedidos de venda, e desenha o que o servidor contar.

import "core:fmt"
import "core:strings"
import rl "vendor:raylib"

MAX_PLAYERS :: 8
MAX_SCHOOLS :: 70
RANK_SIZE :: 10
MAX_SHOTS :: 32
MAX_MARKS :: 103 // cardumes (70) + bags e coroas (32) + a sereia
LOOT_SLOTS :: 4
LOG_SIZE :: 6

Screen :: enum {
	Join,
	Playing,
}

Net_Status :: enum {
	Closed,
	Connecting,
	Open,
}

Player_Info :: struct {
	name_buf: [NAME_MAX_LEN]u8,
	name:     string,
	color:    u8,
}

Boat :: struct {
	id:     u8,
	x, y:   i32,
	look:   u8,
	hp_pct: u8, // vida em %, para a barrinha em cima do barco
}

Loot :: struct {
	kind:      u8, // LOOT_NONE, LOOT_BAG ou LOOT_CROWN
	fish:      int,
	owner_buf: [NAME_MAX_LEN]u8,
	owner_len: int,
}

// Nome de quem morreu e deixou o saque.
loot_owner :: proc(l: ^Loot) -> string {
	return string(l.owner_buf[:l.owner_len])
}

School :: struct {
	x, y:       i32,
	r, species: u8,
}

Mark :: struct {
	x, y:          i32,
	kind, species: u8,
}

Rank_Entry :: struct {
	name_buf: [NAME_MAX_LEN]u8,
	name:     string,
	money:    u32,
	online:   bool,
}

Log_Line :: struct {
	buf:  [128]u8,
	text: string,
	time: f64,
}

Popup :: struct {
	buf:   [48]u8,
	text:  string,
	color: rl.Color,
	time:  f64,
}

State :: struct {
	screen:        Screen,
	net:           Net_Status,
	offline:       bool, // desktop sem servidor: só navegar
	url:           string,
	name_buf:      [NAME_MAX_LEN]u8,
	name_len:      int,
	pass_buf:      [PASSWORD_MAX_LEN]u8,
	pass_len:      int,
	typing_pass:   bool,
	join_message:  string,

	// mundo
	seed:          u32,
	tiles:         [MAP_W * MAP_H]Tile,
	map_ready:     bool,
	map_mismatch:  bool,
	minimap:       rl.Texture2D,

	// eu
	my_id:         u8,
	x, y:          i32,
	money:         u32,
	capacity:      int,
	speed:         int,
	vel:           int,
	at_dock:       bool,
	hp, hp_max:    int,
	broken:        bool,
	safe:          bool, // zona segura: ninguém atira nem toma tiro
	at_mermaid:    bool, // perto da pedra da sereia
	reload:        int,  // ticks até poder atirar de novo
	kills, deaths: int,
	loot:          [LOOT_SLOTS]Loot,
	has_mission:   bool,
	mission_buf:   [NAME_MAX_LEN]u8, // nome do alvo da missão da sereia
	mission_len:   int,
	boat_level:    [PART_COUNT]int,
	hold:          [FISH_COUNT]int,
	bag:           [BAG_SIZE]Item,
	stats:         [STAT_COUNT]int, // do barco: base + itens equipados
	online:        int,
	tick:          u32,
	input_dx:      i8,
	input_dy:      i8,
	fire_tick:     u32, // tick do último tiro mandado, para não mandar dois no mesmo
	offline_move:  f32,

	// o que o servidor mostra
	players:       [MAX_PLAYERS]Player_Info,
	boats:         [MAX_PLAYERS]Boat,
	boat_count:    int,
	schools:       [MAX_SCHOOLS]School,
	school_count:  int,
	shots:         [MAX_SHOTS][2]i32,
	shot_count:    int,
	marks:         [MAX_MARKS]Mark,
	mark_count:    int,
	rank:          [RANK_SIZE]Rank_Entry,
	rank_count:    int,
	prices:        [FISH_COUNT]int,

	// interface
	shop_open:     bool,
	yard_open:     bool,
	inv_open:      bool,
	inv_cursor:    int,
	discard_armed: bool, // apertou X uma vez: o próximo X joga fora
	big_map:       bool,
	log:           [LOG_SIZE]Log_Line,
	log_count:     int,
	popups:        [8]Popup,
	popup_next:    int,
	wake:          [10][2]i32, // rastro do barco
	wake_next:     int,
}

s: State

init :: proc() {
	rl.SetConfigFlags({.WINDOW_RESIZABLE, .VSYNC_HINT})
	rl.InitWindow(1280, 720, "Cananeia Deep Sea")
	rl.SetTargetFPS(60)
	rl.SetExitKey(.KEY_NULL) // Esc fecha a loja, não o jogo
	render_init()
	s.url = net_default_url()
	for &p, i in s.prices do p = SPECIES[i].price_cents
	s.capacity = 150
	s.speed = 5
	s.hp, s.hp_max = 100, 100
}

should_run :: proc() -> bool {
	when ODIN_OS == .JS {
		return true
	} else {
		return !rl.WindowShouldClose()
	}
}

update :: proc() {
	switch s.screen {
	case .Join:
		update_join()
	case .Playing:
		update_playing()
	}

	rl.BeginDrawing()
	rl.ClearBackground(COLOR_BG)
	switch s.screen {
	case .Join:
		draw_join()
	case .Playing:
		draw_world()
		draw_hud()
		if s.shop_open do draw_shop()
		if s.yard_open do draw_shipyard()
		if s.inv_open do draw_inventory()
		if s.big_map do draw_big_map()
	}
	rl.EndDrawing()
	free_all(context.temp_allocator)
}

shutdown :: proc() {
	rl.CloseWindow()
}

// --- tela de entrada ---------------------------------------------------------

@(private = "file")
update_join :: proc() {
	// As teclas vão para o campo que está com o cursor.
	buf := s.pass_buf[:] if s.typing_pass else s.name_buf[:]
	length := &s.pass_len if s.typing_pass else &s.name_len
	for ch := rl.GetCharPressed(); ch != 0; ch = rl.GetCharPressed() {
		if ch < 32 || ch > 126 do continue // só ASCII, por enquanto
		if length^ < len(buf) {
			buf[length^] = u8(ch)
			length^ += 1
		}
	}
	if (rl.IsKeyPressed(.BACKSPACE) || rl.IsKeyPressedRepeat(.BACKSPACE)) && length^ > 0 do length^ -= 1
	if s.url != "" && (rl.IsKeyPressed(.TAB) || rl.IsKeyPressed(.UP) || rl.IsKeyPressed(.DOWN)) do s.typing_pass = !s.typing_pass

	name := strings.trim_space(string(s.name_buf[:s.name_len]))
	if !rl.IsKeyPressed(.ENTER) || len(name) == 0 || s.net == .Connecting do return
	if !s.typing_pass && s.url != "" {
		s.typing_pass = true // Enter no nome pula para a senha
		return
	}
	if s.url != "" && s.pass_len < PASSWORD_MIN_LEN {
		// join_message precisa ser texto fixo: o que o tprintf devolve some no fim do quadro.
		s.join_message = "A senha precisa ter pelo menos 4 caracteres."
		return
	}
	if s.net == .Open {
		send_join(name, string(s.pass_buf[:s.pass_len]))
	} else if net_connect(s.url) {
		s.net = .Connecting
		s.join_message = "Conectando..."
	} else {
		start_offline()
	}
}

@(private = "file")
start_offline :: proc() {
	s.offline = true
	s.join_message = ""
	welcome(1337)
	s.x, s.y = DOCK_X, DOCK_Y + 2
	s.players[0].name = string(s.name_buf[:s.name_len])
	add_log("Modo offline: sem servidor, só dá para navegar.")
}

// --- jogando -------------------------------------------------------------------

@(private = "file")
update_playing :: proc() {
	if rl.IsKeyPressed(.I) {
		s.inv_open = !s.inv_open
		s.shop_open = false
		s.yard_open = false
		s.discard_armed = false
	}
	if s.inv_open do update_inventory()
	dx, dy: i8
	if !s.shop_open && !s.inv_open && !s.yard_open {
		if rl.IsKeyDown(.A) || rl.IsKeyDown(.LEFT) do dx -= 1
		if rl.IsKeyDown(.D) || rl.IsKeyDown(.RIGHT) do dx += 1
		if rl.IsKeyDown(.W) || rl.IsKeyDown(.UP) do dy -= 1
		if rl.IsKeyDown(.S) || rl.IsKeyDown(.DOWN) do dy += 1
	}
	if dx != s.input_dx || dy != s.input_dy {
		s.input_dx, s.input_dy = dx, dy
		send_input(dx, dy)
	}
	if s.offline do offline_move(dx, dy)

	// Tiro: clique (ou Espaço) atira na célula do mouse. Segurando, atira de
	// novo assim que a recarga acaba. Quem confere tudo é o servidor.
	if !s.shop_open && !s.inv_open && !s.yard_open && !s.big_map && !s.offline {
		pressed := rl.IsMouseButtonPressed(.LEFT) || rl.IsKeyPressed(.SPACE)
		held := rl.IsMouseButtonDown(.LEFT) || rl.IsKeyDown(.SPACE)
		_, _, armed := equipped_weapon()
		if pressed && !armed {
			add_log("Sem arma! Equipe um arpão, mosquete ou canhão no inventário (I).")
		} else if pressed && s.safe {
			add_log("Zona segura: perto do porto ninguém atira.")
		} else if held && armed && !s.safe && s.reload == 0 && s.tick != s.fire_tick {
			if mx, my, ok := mouse_cell(); ok {
				send_fire(mx, my)
				s.fire_tick = s.tick
			}
		}
	}

	if rl.IsKeyPressed(.M) do s.big_map = !s.big_map
	if rl.IsKeyPressed(.E) && !s.inv_open {
		if s.shop_open {
			s.shop_open = false
		} else if s.at_dock {
			s.shop_open = true
			s.yard_open = false
		} else {
			add_log("O lojista (L) fica no cais (H) do porto.")
		}
	}
	if rl.IsKeyPressed(.U) && !s.inv_open {
		if s.yard_open {
			s.yard_open = false
		} else if s.at_dock {
			s.yard_open = true
			s.shop_open = false
		} else {
			add_log("O estaleiro (E) fica no cais (H) do porto.")
		}
	}
	if rl.IsKeyPressed(.ESCAPE) {
		s.shop_open = false
		s.yard_open = false
		s.big_map = false
		s.inv_open = false
	}
	if s.shop_open {
		for i in 0 ..< FISH_COUNT {
			if rl.IsKeyPressed(rl.KeyboardKey(int(rl.KeyboardKey.ONE) + i)) do send_sell(u8(i))
		}
		if rl.IsKeyPressed(.T) do send_sell(FISH_NONE)
	}
	if s.yard_open {
		for part in 0 ..< PART_COUNT {
			if rl.IsKeyPressed(rl.KeyboardKey(int(rl.KeyboardKey.ONE) + part)) do send_upgrade(u8(part))
		}
		if rl.IsKeyPressed(.R) do send_repair()
	}
	if !s.at_dock {
		s.shop_open = false
		s.yard_open = false
	}
}

// Inventário aberto: W/S escolhem o item, Enter equipa, X duas vezes joga fora.
@(private = "file")
update_inventory :: proc() {
	step := 0
	if rl.IsKeyPressed(.W) || rl.IsKeyPressed(.UP) || rl.IsKeyPressedRepeat(.W) || rl.IsKeyPressedRepeat(.UP) do step = -1
	if rl.IsKeyPressed(.S) || rl.IsKeyPressed(.DOWN) || rl.IsKeyPressedRepeat(.S) || rl.IsKeyPressedRepeat(.DOWN) do step = 1
	if step != 0 {
		s.inv_cursor = (s.inv_cursor + step + BAG_SIZE) % BAG_SIZE
		s.discard_armed = false
	}
	if s.bag[s.inv_cursor].base == ITEM_NONE {
		s.discard_armed = false
		return
	}
	if rl.IsKeyPressed(.ENTER) || rl.IsKeyPressed(.SPACE) do send_equip(u8(s.inv_cursor))
	if rl.IsKeyPressed(.X) {
		if s.discard_armed do send_discard(u8(s.inv_cursor))
		s.discard_armed = !s.discard_armed
	}
}

// Sem servidor, o próprio cliente anda com o barco (mesma regra do servidor).
@(private = "file")
offline_move :: proc(dx, dy: i8) {
	if dx == 0 && dy == 0 {s.offline_move = 0; return}
	s.offline_move += f32(s.speed) * rl.GetFrameTime()
	for s.offline_move >= 1 {
		s.offline_move -= 1
		nx, ny := s.x + i32(dx), s.y + i32(dy)
		if tile_is_sailable(tile_at(nx, ny)) {
			s.x, s.y = nx, ny
			remember_wake()
		}
	}
	s.at_dock = abs(s.x - DOCK_X) <= 1 && abs(s.y - DOCK_Y) <= 1
}

tile_at :: proc(x, y: i32) -> Tile {
	if x < 0 || y < 0 || x >= MAP_W || y >= MAP_H do return .Mountain
	return s.tiles[y * MAP_W + x]
}

@(private = "file")
remember_wake :: proc() {
	s.wake[s.wake_next] = {s.x, s.y}
	s.wake_next = (s.wake_next + 1) % len(s.wake)
}

add_log :: proc(format: string, args: ..any) {
	if s.log_count == LOG_SIZE {
		for i in 1 ..< LOG_SIZE {
			s.log[i - 1] = s.log[i]
			s.log[i - 1].text = string(s.log[i - 1].buf[:len(s.log[i].text)])
		}
		s.log_count -= 1
	}
	line := &s.log[s.log_count]
	line.text = fmt.bprintf(line.buf[:], format, ..args)
	line.time = rl.GetTime()
	s.log_count += 1
}

@(private = "file")
add_popup :: proc(color: rl.Color, format: string, args: ..any) {
	p := &s.popups[s.popup_next]
	s.popup_next = (s.popup_next + 1) % len(s.popups)
	p.text = fmt.bprintf(p.buf[:], format, ..args)
	p.color = color
	p.time = rl.GetTime()
}

money_text :: proc(cents: u32) -> string {
	return fmt.tprintf("$%d,%02d", cents / 100, cents % 100)
}

// Nome de quem a sereia mandou afundar.
mission_target :: proc() -> string {
	return string(s.mission_buf[:s.mission_len])
}

player_name :: proc(id: int) -> string {
	return s.players[id].name if id >= 0 && id < MAX_PLAYERS else "Alguém"
}

hold_count :: proc() -> int {
	n := 0
	for h in s.hold do n += h
	return n
}

// --- rede ----------------------------------------------------------------------

on_net_open :: proc() {
	s.net = .Open
	s.join_message = ""
	send_join(strings.trim_space(string(s.name_buf[:s.name_len])), string(s.pass_buf[:s.pass_len]))
}

on_net_close :: proc() {
	s.net = .Closed
	s.screen = .Join
	s.shop_open = false
	s.yard_open = false
	s.inv_open = false
	s.input_dx, s.input_dy = 0, 0
	s.join_message = "Conexão perdida. Aperte Enter para tentar de novo."
}

@(private = "file")
welcome :: proc(seed: u32) {
	if !s.map_ready || seed != s.seed {
		s.seed = seed
		world_generate(&s.tiles, seed)
		minimap_build()
		s.map_ready = true
	}
	s.screen = .Playing
}

on_net_message :: proc(data: []u8) {
	r := reader(data)
	switch read_u8(&r) {
	case S_WELCOME:
		s.my_id = read_u8(&r)
		seed := read_u32(&r)
		_ = read_u16(&r) // largura e altura: fixas por enquanto (512)
		_ = read_u16(&r)
		checksum := read_u32(&r)
		if !r.ok do return
		welcome(seed)
		s.map_mismatch = world_checksum(&s.tiles) != checksum
		if s.map_mismatch do add_log("ATENÇÃO: mapa diferente do servidor!")

	case S_ERROR:
		switch read_u8(&r) {
		case ERR_BAD_NAME: s.join_message = "Nome inválido."
		case ERR_NAME_ONLINE: s.join_message = "Esse capitão já está no mar. Use outro nome."
		case ERR_FULL: s.join_message = "Servidor lotado. Tente mais tarde."
		case ERR_VERSION: s.join_message = "Versão do jogo diferente do servidor. Recarregue a página."
		case ERR_BAD_PASSWORD: s.join_message = "A senha precisa ter de 4 a 32 caracteres, sem acento."
		case ERR_WRONG_PASSWORD:
			s.join_message = "Senha errada para esse capitão."
			s.pass_len = 0
			s.typing_pass = true
		}
		s.screen = .Join

	case S_STATE:
		read_state(&r)

	case S_PLAYER:
		id := read_u8(&r)
		color := read_u8(&r)
		if !r.ok || id >= MAX_PLAYERS do return
		p := &s.players[id]
		p.color = color
		p.name = read_name(&r, p.name_buf[:])

	case S_EVENT:
		kind := read_u8(&r)
		species := int(read_u8(&r))
		count := int(read_u16(&r))
		value := read_u32(&r)
		if !r.ok do return
		switch kind {
		case EV_CATCH:
			add_popup(SPECIES[species].color, "+%d %s", count, species_name(species))
			add_log("+%d %s", count, species_name(species))
		case EV_SOLD:
			add_popup(COLOR_ACCENT, "+%s", money_text(value))
			add_log("Vendeu %d peixe(s) por %s.", count, money_text(value))
		case EV_HOLD_FULL:
			add_log("Porão cheio! Volte ao porto para vender.")
		case EV_NOTHING_TO_SELL:
			add_log("Lojista: \"Não tem nada disso no seu porão, marujo.\"")
		case EV_WELCOME:
			add_log("Bem-vindo, %s! No cais (H), aperte E para falar com o lojista (L).", s.players[s.my_id].name)
		case EV_ITEM:
			// No baú, `species` é o tipo do item e `count` a raridade.
			add_popup(rarity_color(u8(count)), "Baú! %s", item_name(u8(species)))
			add_log("Baú! %s (%s). Aperte I para ver.", item_name(u8(species)), rarity_name(u8(count)))
		case EV_BAG_FULL:
			add_log("Achou um baú, mas a bolsa está cheia. Jogue algo fora (I).")
		case EV_CRASH:
			add_popup({255, 107, 107, 255}, "-%d vida", count)
			add_log("Bateu na costa! -%d de vida, sobrou %d. Perto da terra, vá devagar.", count, value)
		case EV_BROKEN:
			add_log("Barco avariado! Só anda devagar. Conserte no estaleiro (U) do porto.")
		case EV_UPGRADED:
			add_popup(COLOR_ACCENT, "%s nível %d!", part_name(species), count + 1)
			add_log("Estaleiro: %s agora é nível %d, por %s.", part_name(species), count + 1, money_text(value))
		case EV_REPAIRED:
			add_log("Estaleiro: consertou %d de vida por %s.", count, money_text(value))
		case EV_NO_MONEY:
			add_log("Estaleiro: \"Isso custa %s, marujo. Volte com o dinheiro.\"", money_text(value))
		case EV_HIT:
			// Nos tiros, `species` é o id do outro jogador.
			add_log("Acertou %s! -%d, sobrou %d de vida.", player_name(species), count, value)
		case EV_HURT:
			add_popup({255, 107, 107, 255}, "-%d vida", count)
			add_log("%s te acertou! -%d de vida, sobrou %d.", player_name(species), count, value)
		case EV_SAFE_ZONE:
			add_log("Zona segura: perto do porto ninguém atira.")
		case EV_KILL:
			add_popup(COLOR_ACCENT, "Afundou %s!", player_name(species))
			add_log("Você afundou %s! A bag e a coroa ficaram boiando.", player_name(species))
		case EV_DIED:
			add_log("%s afundou seu barco! %d peixe(s) ficaram na bag. Você voltou ao porto.", player_name(species), count)
		case EV_LOOT:
			what := "bag" if species == LOOT_BAG else "coroa"
			if count == 1 {
				add_log("Recuperou sua %s!", what)
			} else {
				add_popup(COLOR_ACCENT, "+%s", what)
				add_log("Pegou uma %s! Veja no inventário (I).", what)
			}
		case EV_LOOT_FULL:
			add_log("O barco já carrega %d saques: não cabe mais nenhum.", LOOT_SLOTS)
		}


	case S_RANK:
		n := int(read_u8(&r))
		s.rank_count = 0
		for _ in 0 ..< min(n, RANK_SIZE) {
			e := &s.rank[s.rank_count]
			e.money = read_u32(&r)
			e.online = read_u8(&r) != 0
			e.name = read_name(&r, e.name_buf[:])
			if !r.ok do break
			s.rank_count += 1
		}

	case S_MARKS:
		n := int(read_u8(&r))
		s.mark_count = 0
		for _ in 0 ..< min(n, MAX_MARKS) {
			m := Mark{x = i32(read_u16(&r)), y = i32(read_u16(&r)), kind = read_u8(&r), species = read_u8(&r)}
			if !r.ok do break
			s.marks[s.mark_count] = m
			s.mark_count += 1
		}

	case S_PRICES:
		for &p in s.prices do p = int(read_u16(&r))
	

	case S_INVENTORY:
		// Lê tudo numa cópia e só troca se a mensagem veio inteira.
		n := int(read_u8(&r))
		bag: [BAG_SIZE]Item
		for i in 0 ..< n {
			it := Item{base = read_u8(&r), rarity = read_u8(&r), equipped = read_u8(&r), enchant = read_u8(&r)}
			for &a in it.affix do a = read_u8(&r)
			for &v in it.stats do v = int(read_i16(&r))
			if i < BAG_SIZE do bag[i] = it
		}
		stats: [STAT_COUNT]int
		for &v in stats do v = int(read_i16(&r))
		kills, deaths := int(read_u16(&r)), int(read_u16(&r))
		loot: [LOOT_SLOTS]Loot
		n = int(read_u8(&r))
		for i in 0 ..< n {
			kind, fish := read_u8(&r), int(read_u16(&r))
			name_buf: [NAME_MAX_LEN]u8
			owner := read_name(&r, name_buf[:])
			if i < LOOT_SLOTS do loot[i] = Loot{kind = kind, fish = fish, owner_buf = name_buf, owner_len = len(owner)}
		}
		has_mission := read_u8(&r) != 0
		mission_buf: [NAME_MAX_LEN]u8
		mission := read_name(&r, mission_buf[:])
		if !r.ok do return
		s.bag, s.stats = bag, stats
		s.kills, s.deaths = kills, deaths
		s.loot = loot
		s.has_mission, s.mission_buf, s.mission_len = has_mission, mission_buf, len(mission)

	case S_BOAT:
		levels: [PART_COUNT]int
		for &l in levels do l = int(read_u8(&r))
		if !r.ok do return
		s.boat_level = levels
	}
}

@(private = "file")
read_state :: proc(r: ^Reader) {
	s.tick = read_u32(r)
	x, y := i32(read_u16(r)), i32(read_u16(r))
	if x != s.x || y != s.y {
		s.x, s.y = x, y
		remember_wake()
	}
	s.money = read_u32(r)
	s.capacity = int(read_u16(r))
	s.speed = int(read_u8(r))
	s.vel = int(read_u8(r))
	flags := read_u8(r)
	s.at_dock = flags & STATE_AT_DOCK != 0
	s.broken = flags & STATE_BROKEN != 0
	s.safe = flags & STATE_SAFE != 0
	s.at_mermaid = flags & STATE_AT_MERMAID != 0
	s.hp = int(read_u16(r))
	s.hp_max = int(read_u16(r))
	s.reload = int(read_u8(r))
	for &h in s.hold do h = int(read_u16(r))
	s.online = int(read_u8(r))

	n := int(read_u8(r))
	s.boat_count = 0
	for _ in 0 ..< min(n, MAX_PLAYERS) {
		b := Boat{id = read_u8(r), x = i32(read_u16(r)), y = i32(read_u16(r)), look = read_u8(r), hp_pct = read_u8(r)}
		if !r.ok || b.id >= MAX_PLAYERS do break
		s.boats[s.boat_count] = b
		s.boat_count += 1
	}

	n = int(read_u8(r))
	s.school_count = 0
	for _ in 0 ..< min(n, MAX_SCHOOLS) {
		sc := School{x = i32(read_u16(r)), y = i32(read_u16(r)), r = read_u8(r), species = read_u8(r)}
		if !r.ok do break
		s.schools[s.school_count] = sc
		s.school_count += 1
	}

	n = int(read_u8(r))
	s.shot_count = 0
	for _ in 0 ..< min(n, MAX_SHOTS) {
		shot := [2]i32{i32(read_u16(r)), i32(read_u16(r))}
		if !r.ok do break
		s.shots[s.shot_count] = shot
		s.shot_count += 1
	}
}
