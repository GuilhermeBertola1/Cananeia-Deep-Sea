// Protocolo binário entre cliente e servidor (detalhes em docs/protocol.md).
// Uma mensagem por frame WebSocket binário. Primeiro byte = tipo.
// Números em little-endian. client/src/protocol.odin é o lado do cliente.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define PROTOCOL_VERSION 4
#define NAME_MAX_LEN 16
#define PASSWORD_MIN_LEN 4
#define PASSWORD_MAX_LEN 32

// Cliente -> servidor
enum {
    C_JOIN = 0x01,  // u8 versão, u8 len, nome[len]
    C_INPUT = 0x02, // i8 dx, i8 dy
    C_SELL = 0x03,  // u8 espécie (FISH_NONE = vender tudo)
    C_EQUIP = 0x04,
    C_DISCARD = 0x05,
    C_UPGRADE = 0x06,
    C_REPAIR = 0x07,
};

// Servidor -> cliente
enum {
    S_WELCOME = 0x81, // u8 seu_id, u32 seed, u16 map_w, u16 map_h, u32 checksum, u8 tick_hz
    S_ERROR = 0x82,   // u8 código (ERR_*)
    S_STATE = 0x83,   // estado visível, todo tick
    S_PLAYER = 0x84,  // u8 id, u8 cor, u8 len, nome[len]
    S_EVENT = 0x85,   // u8 tipo (EV_*), u8 espécie, u16 qtd, u32 valor
    S_RANK = 0x86,    // u8 n, n x {u32 dinheiro, u8 online, u8 len, nome[len]}
    S_MARKS = 0x87,   // u8 n, n x {u16 x, u16 y, u8 tipo (MARK_*), u8 extra}
    S_PRICES = 0x88,  // u16 centavos[FISH_COUNT]
    S_INVENTORY = 0x89,
    S_BOAT = 0x8A,
};

enum { ERR_BAD_NAME = 1, ERR_NAME_ONLINE = 2, ERR_FULL = 3, ERR_VERSION = 4, ERR_BAD_PASSWORD = 5, ERR_WRONG_PASSWORD = 6 };
enum { EV_CATCH = 1, EV_SOLD = 2, EV_HOLD_FULL = 3, EV_NOTHING_TO_SELL = 4, EV_WELCOME = 5, EV_ITEM = 6, EV_BAG_FULL = 7,
       EV_CRASH = 8, EV_BROKEN = 9, EV_UPGRADED = 10, EV_REPAIRED = 11, EV_NO_MONEY = 12 };
enum { MARK_SCHOOL = 1 };
enum { STATE_AT_DOCK = 1 << 0, STATE_BROKEN = 1 << 1 };

// Escritor de bytes com limite: se estourar, `ok` vira 0 e nada é enviado.
typedef struct {
    uint8_t *buf;
    size_t cap, len;
    int ok;
} writer_t;

static inline writer_t writer(uint8_t *buf, size_t cap) { return (writer_t){buf, cap, 0, 1}; }

static inline void put_bytes(writer_t *w, const void *src, size_t n) {
    if (!w->ok || w->len + n > w->cap) { w->ok = 0; return; }
    memcpy(w->buf + w->len, src, n);
    w->len += n;
}
static inline void put_u8(writer_t *w, uint8_t v) { put_bytes(w, &v, 1); }
static inline void put_u16(writer_t *w, uint16_t v) { uint8_t b[2] = {(uint8_t)v, (uint8_t)(v >> 8)}; put_bytes(w, b, 2); }
static inline void put_i16(writer_t *w, int16_t v) { put_u16(w, (uint16_t)v); }
static inline void put_u32(writer_t *w, uint32_t v) { uint8_t b[4] = {(uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24)}; put_bytes(w, b, 4); }
static inline void put_name(writer_t *w, const char *name) {
    size_t n = strlen(name);
    put_u8(w, (uint8_t)n);
    put_bytes(w, name, n);
}
