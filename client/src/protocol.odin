package game

// Lado do cliente do protocolo binário (docs/protocol.md e
// server/components/game/include/protocol.h). Little-endian, uma mensagem por frame.

PROTOCOL_VERSION :: 1
NAME_MAX_LEN :: 16

// Cliente -> servidor
C_JOIN :: 0x01
C_INPUT :: 0x02
C_SELL :: 0x03

// Servidor -> cliente
S_WELCOME :: 0x81
S_ERROR :: 0x82
S_STATE :: 0x83
S_PLAYER :: 0x84
S_EVENT :: 0x85
S_RANK :: 0x86
S_MARKS :: 0x87
S_PRICES :: 0x88

ERR_BAD_NAME :: 1
ERR_NAME_ONLINE :: 2
ERR_FULL :: 3
ERR_VERSION :: 4

EV_CATCH :: 1
EV_SOLD :: 2
EV_HOLD_FULL :: 3
EV_NOTHING_TO_SELL :: 4
EV_WELCOME :: 5

MARK_SCHOOL :: 1
STATE_AT_DOCK :: 1 << 0

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

send_join :: proc(name: string) {
	buf: [3 + NAME_MAX_LEN]u8
	buf[0] = C_JOIN
	buf[1] = PROTOCOL_VERSION
	n := copy(buf[3:], name)
	buf[2] = u8(n)
	net_send(buf[:3 + n])
}

send_input :: proc(dx, dy: i8) {
	buf := [3]u8{C_INPUT, u8(dx), u8(dy)}
	net_send(buf[:])
}

send_sell :: proc(species: u8) {
	buf := [2]u8{C_SELL, species}
	net_send(buf[:])
}
