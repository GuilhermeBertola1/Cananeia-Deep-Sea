// Protocolo binário entre cliente e servidor (detalhes em docs/protocol.md).
// Uma mensagem por frame WebSocket binário. Primeiro byte = tipo.
// Números em little-endian. client/src/protocol.odin é o lado do cliente.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define PROTOCOL_VERSION 11
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
    C_FIRE = 0x08,
    C_MERC = 0x09,
    C_MERMAID = 0x0A, // u8 ação (MERMAID_*)
    C_COMBINE = 0x0B, // u8 índice a, u8 índice b
    C_BUY = 0x0C,     // u8 loja (SHOP_*), u8 oferta
    C_DELIVER = 0x0D, // entrega a encomenda do mascate da vila onde está
    C_WAYPOINT = 0x0E, // u8 índice, u16 x, u16 y: marca o waypoint (x = y = 0 apaga)
};

// Servidor -> cliente
enum {
    S_WELCOME = 0x81, // u8 seu_id, u32 seed, u32 map_w, u32 map_h, u8 tick_hz, u8 n, n x {u16 rx, u16 ry, u32 checksum}
    S_ERROR = 0x82,   // u8 código (ERR_*)
    S_STATE = 0x83,   // estado visível, todo tick
    S_PLAYER = 0x84,  // u8 id, u8 cor, u8 len, nome[len]
    S_EVENT = 0x85,   // u8 tipo (EV_*), u8 espécie, u16 qtd, u32 valor
    S_RANK = 0x86,    // u8 n, n x {u32 dinheiro, u8 online, u8 len, nome[len]}
    S_MARKS = 0x87,   // u8 n, n x {u16 x, u16 y, u8 tipo (MARK_*), u8 extra}
    S_PRICES = 0x88,  // u8 balcão (0 = porto, 1 + v = vila v), u16 centavos[FISH_COUNT]
    S_INVENTORY = 0x89,
    S_BOAT = 0x8A,
    S_CATALOG = 0x8B, // catálogo do mascate da vila onde o barco está
    S_WAYPOINTS = 0x8C, // u8 n, n x {u16 x, u16 y}: os waypoints do jogador ((0, 0) = vazio)
};

// Waypoints: pontos que cada capitão marca no mapa, só ele vê. Ficam no perfil.
#define WAYPOINT_COUNT 8

enum { ERR_BAD_NAME = 1, ERR_NAME_ONLINE = 2, ERR_FULL = 3, ERR_VERSION = 4, ERR_BAD_PASSWORD = 5, ERR_WRONG_PASSWORD = 6 };
enum { EV_CATCH = 1, EV_SOLD = 2, EV_HOLD_FULL = 3, EV_NOTHING_TO_SELL = 4, EV_WELCOME = 5, EV_ITEM = 6, EV_BAG_FULL = 7,
       EV_CRASH = 8, EV_BROKEN = 9, EV_UPGRADED = 10, EV_REPAIRED = 11, EV_NO_MONEY = 12,
       EV_HIT = 13, EV_HURT = 14, EV_SAFE_ZONE = 15,
       EV_KILL = 16, EV_DIED = 17, EV_LOOT = 18, EV_LOOT_FULL = 19,
       EV_MERC = 20, EV_MERC_NOTHING = 21,
       EV_SPIN_WAIT = 22, EV_SPIN_STEAL = 23, EV_SPIN_MISSION = 24, EV_HUNTED = 25, EV_SPIN_NOTHING = 26,
       EV_MISSION_WAIT = 27, EV_ENCHANT = 28, EV_NOTHING_TO_ENCHANT = 29, EV_MISSION_GIVE_UP = 30,
       EV_COMBINED = 31, EV_CANT_COMBINE = 32,
       EV_BITTEN = 33, EV_PREDATOR_HIT = 34, EV_PREDATOR_KILL = 35,
       EV_WORLD_START = 36, EV_WORLD_END = 37,
       EV_BOSS_START = 38, EV_BOSS_HIT = 39, EV_BOSS_END = 40, EV_BOSS_REWARD = 41,
       EV_WEATHER = 42, EV_BOUGHT = 43,
       EV_SOLD_OUT = 44, EV_CATALOG = 45, EV_QUEST_DONE = 46, EV_QUEST_WAIT = 47, EV_QUEST_MISSING = 48 };
// Lojas do C_BUY: o armeiro, e (a partir do passo 4) o mascate de cada vila.
enum { SHOP_ARMORY = 0, SHOP_VILLAGE = 1 };
// Eventos do mundo (no EV_WORLD_START, `espécie` é o tipo).
enum { WORLD_NONE = 0, WORLD_STORM = 1, WORLD_SQUID = 2, WORLD_GHOST = 3 };
// Clima, no mapa todo (no EV_WEATHER, `espécie` é o clima novo e `quantidade`, os segundos).
enum { WEATHER_CLEAR = 0, WEATHER_RAIN = 1, WEATHER_FOG = 2 };
// Tipos de bicho (no EV_BITTEN, EV_PREDATOR_* e somados a KILLER_PREDATOR).
enum { PRED_SHARK = 0, PRED_GHOST = 1 };
// Bosses (no EV_BOSS_*, `espécie` é o tipo).
enum { BOSS_NONE = 0, BOSS_MEGALODON = 1, BOSS_KRAKEN = 2, BOSS_CTHULHU = 3, BOSS_COUNT };
// No EV_DIED, `espécie` é quem afundou: o id do jogador, KILLER_PREDATOR + tipo do bicho
// ou KILLER_BOSS + tipo do boss.
#define KILLER_PREDATOR 0x80
#define KILLER_BOSS 0xA0
enum { MARK_SCHOOL = 1, MARK_BAG = 2, MARK_CROWN = 3, MARK_MERMAID = 4, MARK_VILLAGE = 5, MARK_ARMORY = 6 };
enum { MERMAID_TALK = 0, MERMAID_GIVE_UP = 1 };
enum { STATE_AT_DOCK = 1 << 0, STATE_BROKEN = 1 << 1, STATE_SAFE = 1 << 2, STATE_AT_MERMAID = 1 << 3,
       STATE_AT_VILLAGE = 1 << 4, STATE_GRABBED = 1 << 5, STATE_MAD = 1 << 6, STATE_AT_ARMORY = 1 << 7 };

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
