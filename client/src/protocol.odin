package game

// Lado do cliente do protocolo binário (docs/protocol.md e
// server/components/game/include/protocol.h). Little-endian, uma mensagem por frame.

PROTOCOL_VERSION :: 11
NAME_MAX_LEN :: 16
PASSWORD_MIN_LEN :: 4
PASSWORD_MAX_LEN :: 32

// Cliente -> servidor
C_JOIN :: 0x01
C_INPUT :: 0x02
C_SELL :: 0x03
C_EQUIP :: 0x04
C_DISCARD :: 0x05
C_UPGRADE :: 0x06
C_REPAIR :: 0x07
C_FIRE :: 0x08
C_MERC :: 0x09
C_MERMAID :: 0x0A
C_COMBINE :: 0x0B
C_BUY :: 0x0C
C_DELIVER :: 0x0D
C_WAYPOINT :: 0x0E

// Servidor -> cliente
S_WELCOME :: 0x81
S_ERROR :: 0x82
S_STATE :: 0x83
S_PLAYER :: 0x84
S_EVENT :: 0x85
S_RANK :: 0x86
S_MARKS :: 0x87
S_PRICES :: 0x88
S_INVENTORY :: 0x89
S_BOAT :: 0x8A
S_CATALOG :: 0x8B
S_WAYPOINTS :: 0x8C

WAYPOINT_COUNT :: 8

ERR_BAD_NAME :: 1
ERR_NAME_ONLINE :: 2
ERR_FULL :: 3
ERR_VERSION :: 4
ERR_BAD_PASSWORD :: 5
ERR_WRONG_PASSWORD :: 6

EV_CATCH :: 1
EV_SOLD :: 2
EV_HOLD_FULL :: 3
EV_NOTHING_TO_SELL :: 4
EV_WELCOME :: 5
EV_ITEM :: 6
EV_BAG_FULL :: 7
EV_CRASH :: 8
EV_BROKEN :: 9
EV_UPGRADED :: 10
EV_REPAIRED :: 11
EV_NO_MONEY :: 12
EV_HIT :: 13
EV_HURT :: 14
EV_SAFE_ZONE :: 15
EV_KILL :: 16
EV_DIED :: 17
EV_LOOT :: 18
EV_LOOT_FULL :: 19
EV_MERC :: 20
EV_MERC_NOTHING :: 21
EV_SPIN_WAIT :: 22
EV_SPIN_STEAL :: 23
EV_SPIN_MISSION :: 24
EV_HUNTED :: 25
EV_SPIN_NOTHING :: 26
EV_MISSION_WAIT :: 27
EV_ENCHANT :: 28
EV_NOTHING_TO_ENCHANT :: 29
EV_MISSION_GIVE_UP :: 30
EV_COMBINED :: 31
EV_CANT_COMBINE :: 32
EV_BITTEN :: 33
EV_PREDATOR_HIT :: 34
EV_PREDATOR_KILL :: 35
EV_WORLD_START :: 36
EV_WORLD_END :: 37
EV_BOSS_START :: 38
EV_BOSS_HIT :: 39
EV_BOSS_END :: 40
EV_BOSS_REWARD :: 41
EV_WEATHER :: 42
EV_BOUGHT :: 43
EV_SOLD_OUT :: 44
EV_CATALOG :: 45
EV_QUEST_DONE :: 46
EV_QUEST_WAIT :: 47
EV_QUEST_MISSING :: 48

// Lojas do C_BUY: o armeiro, e o mascate da vila v é SHOP_VILLAGE + v.
SHOP_ARMORY :: 0
SHOP_VILLAGE :: 1

// No EV_DIED e no EV_HURT, quem atacou: id do jogador, KILLER_PREDATOR + tipo do bicho
// ou KILLER_BOSS + tipo do boss.
KILLER_PREDATOR :: 0x80
KILLER_BOSS :: 0xA0
PRED_SHARK :: 0
PRED_GHOST :: 1

WORLD_NONE :: 0
WORLD_STORM :: 1
WORLD_SQUID :: 2
WORLD_GHOST :: 3

// Clima, no mapa todo.
WEATHER_CLEAR :: 0
WEATHER_RAIN :: 1
WEATHER_FOG :: 2

BOSS_NONE :: 0
BOSS_MEGALODON :: 1
BOSS_KRAKEN :: 2
BOSS_CTHULHU :: 3

MARK_SCHOOL :: 1
MARK_BAG :: 2
MARK_CROWN :: 3
MARK_MERMAID :: 4
MARK_VILLAGE :: 5
MARK_ARMORY :: 6
STATE_AT_DOCK :: 1 << 0
STATE_BROKEN :: 1 << 1
STATE_SAFE :: 1 << 2
STATE_AT_MERMAID :: 1 << 3
STATE_AT_VILLAGE :: 1 << 4
STATE_GRABBED :: 1 << 5 // preso por um tentáculo
STATE_MAD :: 1 << 6 // louco perto do Cthulhu: o leme está invertido
STATE_AT_ARMORY :: 1 << 7

MERMAID_TALK :: 0
MERMAID_GIVE_UP :: 1

LOOT_NONE :: 0
LOOT_BAG :: 1
LOOT_CROWN :: 2

// Leitor com limite: se a mensagem vier curta, `ok` vira false e tudo lê 0.
Reader :: struct {
	data: []u8,
	pos:  int,
	ok:   bool,
}

reader :: proc(data: []u8) -> Reader {return {data, 0, true}}

read_u8 :: proc(r: ^Reader) -> u8 {
	if r.pos + 1 > len(r.data) {r.ok = false; return 0}
	r.pos += 1
	return r.data[r.pos - 1]
}

read_u16 :: proc(r: ^Reader) -> u16 {
	lo := u16(read_u8(r))
	return lo | u16(read_u8(r)) << 8
}

read_i16 :: proc(r: ^Reader) -> i16 {
	return i16(read_u16(r)) // mesmos 16 bits, lidos com sinal
}

read_u32 :: proc(r: ^Reader) -> u32 {
	lo := u32(read_u16(r))
	return lo | u32(read_u16(r)) << 16
}

// Copia o nome para `buf` e devolve a fatia usada.
read_name :: proc(r: ^Reader, buf: []u8) -> string {
	n := int(read_u8(r))
	if r.pos + n > len(r.data) {r.ok = false; return ""}
	n2 := copy(buf, r.data[r.pos:r.pos + n])
	r.pos += n
	return string(buf[:n2])
}

send_join :: proc(name, password: string) {
	buf: [4 + NAME_MAX_LEN + PASSWORD_MAX_LEN]u8
	buf[0] = C_JOIN
	buf[1] = PROTOCOL_VERSION
	n := copy(buf[3:3 + NAME_MAX_LEN], name)
	buf[2] = u8(n)
	pos := 3 + n
	p := copy(buf[pos + 1:pos + 1 + PASSWORD_MAX_LEN], password)
	buf[pos] = u8(p)
	net_send(buf[:pos + 1 + p])
}

send_input :: proc(dx, dy: i8) {
	buf := [3]u8{C_INPUT, u8(dx), u8(dy)}
	net_send(buf[:])
}

send_sell :: proc(species: u8) {
	buf := [2]u8{C_SELL, species}
	net_send(buf[:])
}

send_equip :: proc(bag_index: u8) {
	buf := [2]u8{C_EQUIP, bag_index}
	net_send(buf[:])
}

send_discard :: proc(bag_index: u8) {
	buf := [2]u8{C_DISCARD, bag_index}
	net_send(buf[:])
}

send_upgrade :: proc(part: u8) {
	buf := [2]u8{C_UPGRADE, part}
	net_send(buf[:])
}

send_repair :: proc() {
	buf := [1]u8{C_REPAIR}
	net_send(buf[:])
}

// Atira na célula (x, y) do mundo. O servidor confere arma, recarga e zona segura.
send_fire :: proc(x, y: i32) {
	if x < 0 || y < 0 do return
	buf := [5]u8{C_FIRE, u8(x), u8(x >> 8), u8(y), u8(y >> 8)}
	net_send(buf[:])
}

send_merc :: proc() {
	buf := [1]u8{C_MERC}
	net_send(buf[:])
}

// Fala com a sereia: MERMAID_TALK gira a roleta ou entrega a coroa; MERMAID_GIVE_UP desiste da missão.
send_mermaid :: proc(action: u8) {
	buf := [2]u8{C_MERMAID, action}
	net_send(buf[:])
}

send_combine :: proc(a, b: u8) {
	buf := [3]u8{C_COMBINE, a, b}
	net_send(buf[:])
}

// Compra a oferta `offer` da loja `shop` (SHOP_ARMORY ou SHOP_VILLAGE + vila).
send_buy :: proc(shop, offer: u8) {
	buf := [3]u8{C_BUY, shop, offer}
	net_send(buf[:])
}

// Entrega a encomenda do mascate da vila onde o barco está.
send_deliver :: proc() {
	buf := [1]u8{C_DELIVER}
	net_send(buf[:])
}

// Marca o waypoint `index` em (x, y). (0, 0) apaga.
send_waypoint :: proc(index: u8, x, y: i32) {
	buf := [6]u8{C_WAYPOINT, index, u8(x), u8(x >> 8), u8(y), u8(y >> 8)}
	net_send(buf[:])
}
