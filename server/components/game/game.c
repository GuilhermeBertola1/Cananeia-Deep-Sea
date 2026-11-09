#include "game.h"
#include "world.h"
#include "fish.h"
#include "protocol.h"
#include "storage.h"
#include "items.h"
#include "password.h"
#include "boat.h"
#include "weapon.h"

#define MAX_SCHOOLS 70
#define PASSIVE_CATCH_PCT 6 // chance por tick em movimento
#define SCHOOL_CATCH_PCT 35 // chance por tick dentro do cardume
#define BASE_SPEED 5 // células por segundo
#define MAX_SPEED 16
#define BASE_HP 100
#define CRASH_MIN_SPEED 4
#define CRASH_DAMAGE 2
#define BROKEN_SPEED 2
#define CHEST_PER_MILLE 4
#define SAVE_EVERY (30 * TICK_HZ)
#define RANK_SIZE 10
#define MAX_SHOTS 32
#define MAX_DROPS 32
#define DROP_LIFE (5 * 60 * TICK_HZ) // bag e coroa somem do mar em 5 minutos
// Posição do tiro em 1/256 de célula, para andar em qualquer ângulo só com inteiros.
#define SUB 256
#define CROWN_BOUNTY_CENTS 2500 // o mercenário paga $25 por coroa
// A sereia mora numa pedra em mar aberto, longe do porto.
#define MERMAID_MIN_DIST 70
#define MERMAID_MAX_DIST 110
#define MERMAID_REACH 3   // até quantas células dá para falar com ela
#define MERMAID_SAFE_R 8  // zona segura em volta da pedra
#define SPIN_COOLDOWN (60 * TICK_HZ) // uma volta na roleta por minuto
#define STEAL_PCT 10
#define STEAL_MAX_CENTS 10000 // ela rouba 10% do dinheiro, no máximo $100

// Mecânico: preço pela raridade mínima do item que sai. Nunca sai comum:
// dois comuns já dão um raro.
static const uint32_t COMBINE_PRICE[RARITY_COUNT] = {0, 8000, 25000, 60000};

// Mercado: vender 200 de uma espécie derruba o preço pela metade,
// nunca abaixo de 30%, e a pressão cai 1% por segundo.
#define MARKET_SATURATION 200000 // em milésimos de peixe
#define MARKET_MIN_PCT 30

typedef enum { SLOT_FREE = 0, SLOT_CONNECTED, SLOT_PLAYING } slot_state_t;

typedef struct {
    slot_state_t state;
    int profile; // índice em g.profiles
    int16_t x, y;
    int8_t dx, dy;       // para onde o jogador quer ir (teclado)
    uint8_t heading;     // para onde a proa aponta (0 a 7, ver DIR_X)
    uint8_t turn_timer;  // ticks desde a última virada de 45°
    uint8_t color;
    uint8_t speed;       // velocidade máxima, células por segundo
    uint8_t vel;         // velocidade agora: sobe até `speed` e cai devagar
    uint8_t move_budget; // em décimos de célula
    uint8_t blocked;
    uint8_t reload;
    uint32_t next_spin; // tick em que a roleta da sereia libera de novo
    uint16_t capacity;
    int16_t stats[STAT_COUNT];
} player_t;

typedef struct {
    uint8_t alive;
    uint16_t x, y;
    uint8_t r, species;
    uint32_t expires;
} school_t;

typedef struct {
    uint8_t alive;
    uint8_t owner;       // slot de quem atirou
    uint8_t speed;       // células por tick
    uint8_t left;        // células que ainda pode andar
    int32_t x, y;        // em 1/SUB de célula
    int32_t ux, uy;      // um passo de 1 célula na direção do tiro, em 1/SUB
    uint16_t damage;
} shot_t;

// Bag ou coroa boiando no mar, esperando alguém passar por cima.
typedef struct {
    uint8_t alive;
    uint16_t x, y;
    uint32_t expires;
    loot_t loot;
} drop_t;

// Lugares com NPC: perto deles ninguém toma tiro. O porto e a pedra da
// sereia (que só se sabe onde fica depois de gerar o mapa, em game_init).
typedef struct {
    int16_t x, y, r;
} safe_zone_t;

enum { ZONE_PORT = 0, ZONE_MERMAID, SAFE_ZONE_COUNT };

static struct {
    uint8_t *tiles;
    uint32_t seed, checksum, tick, rng;
    game_send_fn send;
    player_t players[MAX_PLAYERS];
    school_t schools[MAX_SCHOOLS];
    shot_t shots[MAX_SHOTS];
    drop_t drops[MAX_DROPS];
    profile_t profiles[MAX_PROFILES];
    uint32_t pressure[FISH_COUNT];
    safe_zone_t safe_zones[SAFE_ZONE_COUNT];
} g;

static uint8_t out[1024];

// --- utilidades -------------------------------------------------------------

static uint32_t rnd(void) {
    uint32_t x = g.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return g.rng = x;
}
static uint32_t rnd_below(uint32_t n) { return rnd() % n; }
static int chance_permille(uint32_t pm) { return rnd_below(1000) < pm; }

static tile_t tile(int32_t x, int32_t y) {
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return TILE_MOUNTAIN;
    return (tile_t)g.tiles[y * MAP_W + x];
}

static profile_t *prof(const player_t *p) { return &g.profiles[p->profile]; }
static int level(const profile_t *pr, int part) {
    return pr->boat[part] < BOAT_LEVELS ? pr->boat[part] : BOAT_LEVELS - 1;
}

static int hp_max(const player_t *p) { return p->stats[STAT_HP] > 1 ? p->stats[STAT_HP] : 1; }
static int hp(const player_t *p) {
    int h = hp_max(p) - prof(p)->damage;
    return h > 0 ? h : 0;
}

static int hold_count(const player_t *p) {
    int n = 0;
    for (int i = 0; i < FISH_COUNT; i++) n += prof(p)->hold[i];
    return n;
}

static int at_dock(const player_t *p) {
    return p->x >= DOCK_X - 1 && p->x <= DOCK_X + 1 && p->y >= DOCK_Y - 1 && p->y <= DOCK_Y + 1;
}

static int in_safe_zone(int x, int y) {
    for (int i = 0; i < SAFE_ZONE_COUNT; i++) {
        const safe_zone_t *z = &g.safe_zones[i];
        int dx = x - z->x, dy = y - z->y;
        if (dx * dx + dy * dy <= z->r * z->r) return 1;
    }
    return 0;
}

static int near_mermaid(const player_t *p) {
    const safe_zone_t *z = &g.safe_zones[ZONE_MERMAID];
    return p->x >= z->x - MERMAID_REACH && p->x <= z->x + MERMAID_REACH && p->y >= z->y - MERMAID_REACH &&
           p->y <= z->y + MERMAID_REACH;
}

// Raiz quadrada inteira (arredonda para baixo).
static uint32_t isqrt(uint32_t n) {
    uint32_t r = 0, bit = 1u << 30;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) {
            n -= r + bit;
            r = (r >> 1) + bit;
        } else {
            r >>= 1;
        }
        bit >>= 2;
    }
    return r;
}

// Célula onde o tiro está (arredonda para a mais perto).
static int shot_cell(int32_t v) { return (v + SUB / 2) / SUB; }

static uint32_t price_cents(int sp) {
    uint32_t base = SPECIES[sp].price_cents;
    uint32_t p = (uint32_t)((uint64_t)base * MARKET_SATURATION / (MARKET_SATURATION + g.pressure[sp]));
    uint32_t min = base * MARKET_MIN_PCT / 100;
    return p < min ? min : p;
}

static int same_name(const char *a, const char *b) {
    for (;; a++, b++) {
        char ca = (*a >= 'A' && *a <= 'Z') ? *a + 32 : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? *b + 32 : *b;
        if (ca != cb) return 0;
        if (!ca) return 1;
    }
}

// --- envio ------------------------------------------------------------------

static void send_w(int slot, const writer_t *w) {
    if (w->ok) g.send(slot, w->buf, w->len);
}

static void send_all(const writer_t *w) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) send_w(i, w);
}

static void send_error(int slot, uint8_t code) {
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_ERROR);
    put_u8(&w, code);
    send_w(slot, &w);
}

static void send_event(int slot, uint8_t kind, uint8_t species, uint16_t count, uint32_t value) {
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_EVENT);
    put_u8(&w, kind);
    put_u8(&w, species);
    put_u16(&w, count);
    put_u32(&w, value);
    send_w(slot, &w);
}

static void write_player_info(writer_t *w, int id) {
    put_u8(w, S_PLAYER);
    put_u8(w, (uint8_t)id);
    put_u8(w, g.players[id].color);
    put_name(w, prof(&g.players[id])->name);
}

static void send_prices(int slot) {
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_PRICES);
    for (int i = 0; i < FISH_COUNT; i++) put_u16(&w, (uint16_t)price_cents(i));
    slot < 0 ? send_all(&w) : send_w(slot, &w);
}

static void send_marks(int slot) {
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_MARKS);
    size_t count_at = w.len;
    put_u8(&w, 0);
    uint8_t n = 0;
    for (int i = 0; i < MAX_SCHOOLS; i++) {
        const school_t *s = &g.schools[i];
        if (!s->alive) continue;
        put_u16(&w, s->x);
        put_u16(&w, s->y);
        put_u8(&w, MARK_SCHOOL);
        put_u8(&w, s->species);
        n++;
    }
    for (int i = 0; i < MAX_DROPS; i++) {
        const drop_t *d = &g.drops[i];
        if (!d->alive) continue;
        put_u16(&w, d->x);
        put_u16(&w, d->y);
        put_u8(&w, d->loot.kind == LOOT_BAG ? MARK_BAG : MARK_CROWN);
        put_u8(&w, 0);
        n++;
    }
    put_u16(&w, (uint16_t)g.safe_zones[ZONE_MERMAID].x);
    put_u16(&w, (uint16_t)g.safe_zones[ZONE_MERMAID].y);
    put_u8(&w, MARK_MERMAID);
    put_u8(&w, 0);
    n++;
    out[count_at] = n;
    slot < 0 ? send_all(&w) : send_w(slot, &w);
}

// Em que slot do barco o item da bolsa está (0 = não está equipado).
static uint8_t equipped_slot(const profile_t *pr, int bag_index) {
    for (int e = 0; e < EQUIP_SLOTS; e++)
        if (pr->equipped[e] == bag_index + 1) return (uint8_t)(e + 1);
    return 0;
}

// Item da bolsa já com o encanto da sereia.
static item_info_t bag_info(const profile_t *pr, int index) {
    return item_info_enchanted(pr->bag[index], pr->enchant[index]);
}

// O servidor manda o item já "aberto" (raridade, afixos, números), então o
// cliente não precisa rodar o gerador.
static void send_inventory(int slot) {
    const player_t *p = &g.players[slot];
    const profile_t *pr = prof(p);
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_INVENTORY);
    put_u8(&w, BAG_SIZE);
    for (int i = 0; i < BAG_SIZE; i++) {
        item_info_t info = bag_info(pr, i);
        put_u8(&w, info.base);
        put_u8(&w, info.rarity);
        put_u8(&w, equipped_slot(pr, i));
        put_u8(&w, pr->enchant[i]);
        for (int a = 0; a < MAX_AFFIXES; a++) put_u8(&w, info.affix[a]);
        for (int s = 0; s < STAT_COUNT; s++) put_i16(&w, info.stats[s]);
    }
    for (int s = 0; s < STAT_COUNT; s++) put_i16(&w, p->stats[s]);
    put_u16(&w, pr->kills);
    put_u16(&w, pr->deaths);
    put_u8(&w, LOOT_SLOTS);
    for (int i = 0; i < LOOT_SLOTS; i++) {
        const loot_t *l = &pr->loot[i];
        uint16_t fish = 0;
        for (int f = 0; f < FISH_COUNT; f++) fish += l->fish[f];
        put_u8(&w, l->kind);
        put_u16(&w, fish);
        put_name(&w, l->kind ? g.profiles[l->owner].name : "");
    }
    put_u8(&w, pr->has_mission);
    put_name(&w, pr->has_mission ? g.profiles[pr->mission_target].name : "");
    send_w(slot, &w);
}

static int profile_online(int index) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING && g.players[i].profile == index) return 1;
    return 0;
}

// Ranking = todos os perfis salvos (online ou não), os 10 mais ricos.
static void send_rank(int slot) {
    uint8_t picked[MAX_PROFILES] = {0};
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_RANK);
    size_t count_at = w.len;
    put_u8(&w, 0);
    uint8_t n = 0;
    for (; n < RANK_SIZE; n++) {
        int best = -1;
        for (int i = 0; i < MAX_PROFILES; i++) {
            if (!g.profiles[i].used || picked[i]) continue;
            if (best < 0 || g.profiles[i].money > g.profiles[best].money) best = i;
        }
        if (best < 0) break;
        picked[best] = 1;
        put_u32(&w, g.profiles[best].money);
        put_u8(&w, (uint8_t)profile_online(best));
        put_name(&w, g.profiles[best].name);
    }
    out[count_at] = n;
    slot < 0 ? send_all(&w) : send_w(slot, &w);
}

static uint8_t boat_look(const profile_t *pr) {
    return (uint8_t)(level(pr, PART_HULL) << 4 | level(pr, PART_SAIL));
}

static void send_boat(int slot) {
    const profile_t *pr = prof(&g.players[slot]);
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_BOAT);
    for (int i = 0; i < PART_COUNT; i++) put_u8(&w, (uint8_t)level(pr, i));
    send_w(slot, &w);
}

static int near(int x, int y, int ox, int oy, int extra) {
    int dx = x - ox, dy = y - oy;
    return dx >= -(VIEW_RADIUS + extra) && dx <= VIEW_RADIUS + extra && dy >= -(VIEW_RADIUS + extra) &&
           dy <= VIEW_RADIUS + extra;
}

static void send_state(int slot) {
    const player_t *p = &g.players[slot];
    const profile_t *pr = prof(p);
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_STATE);
    put_u32(&w, g.tick);
    put_u16(&w, (uint16_t)p->x);
    put_u16(&w, (uint16_t)p->y);
    put_u32(&w, pr->money);
    put_u16(&w, p->capacity);
    put_u8(&w, p->speed);
    put_u8(&w, p->vel);
    put_u8(&w, (at_dock(p) ? STATE_AT_DOCK : 0) | (hp(p) == 0 ? STATE_BROKEN : 0) |
                   (in_safe_zone(p->x, p->y) ? STATE_SAFE : 0) | (near_mermaid(p) ? STATE_AT_MERMAID : 0));
    put_u16(&w, (uint16_t)hp(p));
    put_u16(&w, (uint16_t)hp_max(p));
    put_u8(&w, p->reload);
    for (int i = 0; i < FISH_COUNT; i++) put_u16(&w, pr->hold[i]);

    uint8_t online = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) online += g.players[i].state == SLOT_PLAYING;
    put_u8(&w, online);

    size_t count_at = w.len;
    put_u8(&w, 0);
    uint8_t n = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const player_t *o = &g.players[i];
        if (i == slot || o->state != SLOT_PLAYING || !near(o->x, o->y, p->x, p->y, 0)) continue;
        put_u8(&w, (uint8_t)i);
        put_u16(&w, (uint16_t)o->x);
        put_u16(&w, (uint16_t)o->y);
        put_u8(&w, boat_look(prof(o)));
        put_u8(&w, (uint8_t)(hp(o) * 100 / hp_max(o)));
        n++;
    }
    out[count_at] = n;

    count_at = w.len;
    put_u8(&w, 0);
    n = 0;
    for (int i = 0; i < MAX_SCHOOLS; i++) {
        const school_t *s = &g.schools[i];
        if (!s->alive || !near(s->x, s->y, p->x, p->y, s->r)) continue;
        put_u16(&w, s->x);
        put_u16(&w, s->y);
        put_u8(&w, s->r);
        put_u8(&w, s->species);
        n++;
    }
    out[count_at] = n;

    count_at = w.len;
    put_u8(&w, 0);
    n = 0;
    for (int i = 0; i < MAX_SHOTS; i++) {
        const shot_t *sh = &g.shots[i];
        int x = shot_cell(sh->x), y = shot_cell(sh->y);
        if (!sh->alive || !near(x, y, p->x, p->y, 0)) continue;
        put_u16(&w, (uint16_t)x);
        put_u16(&w, (uint16_t)y);
        n++;
    }
    out[count_at] = n;
    send_w(slot, &w);
}

// --- regras -----------------------------------------------------------------

// Stats do barco = base + tudo o que está equipado.
static void update_stats(player_t *p) {
    const profile_t *pr = prof(p);
    memset(p->stats, 0, sizeof p->stats);
    p->stats[STAT_SPEED] = BASE_SPEED + SAIL_SPEED[level(pr, PART_SAIL)];
    p->stats[STAT_HP] = BASE_HP + HULL_HP[level(pr, PART_HULL)];
    p->capacity = HULL_CAPACITY[level(pr, PART_HULL)];
    for (int e = 0; e < EQUIP_SLOTS; e++) {
        if (!pr->equipped[e]) continue;
        item_info_t info = bag_info(pr, pr->equipped[e] - 1);
        for (int s = 0; s < STAT_COUNT; s++) p->stats[s] += info.stats[s];
    }
    int speed = p->stats[STAT_SPEED];
    if (hp(p) == 0 && speed > BROKEN_SPEED) speed = BROKEN_SPEED;
    p->speed = (uint8_t)(speed < 1 ? 1 : speed > MAX_SPEED ? MAX_SPEED : speed);
}

// Item novo na bolsa. Devolve o índice, ou -1 se a bolsa está cheia.
static int bag_add(profile_t *pr, item_t item) {
    for (int i = 0; i < BAG_SIZE; i++) {
        if (pr->bag[i].base == ITEM_NONE) {
            pr->bag[i] = item;
            pr->enchant[i] = 0;
            return i;
        }
    }
    return -1;
}

// Baú: um item aleatório de qualquer tipo.
static void give_random_item(int slot) {
    player_t *p = &g.players[slot];
    item_t item = item_new((uint8_t)(1 + rnd_below(ITEM_BASE_COUNT - 1)), rnd());
    if (bag_add(prof(p), item) < 0) {
        send_event(slot, EV_BAG_FULL, FISH_NONE, 0, 0);
        return;
    }
    send_event(slot, EV_ITEM, item.base, item_info(item).rarity, 0);
    send_inventory(slot);
}

static void equip(int slot, uint8_t index) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (index >= BAG_SIZE || pr->bag[index].base == ITEM_NONE) return;
    uint8_t current = equipped_slot(pr, index);
    if (current) {
        pr->equipped[current - 1] = 0; // já estava: desequipa
    } else {
        uint8_t kind = ITEM_BASES[pr->bag[index].base].kind;
        int e = kind == KIND_WEAPON ? EQUIP_WEAPON : kind == KIND_TOOL ? EQUIP_TOOL : EQUIP_BOOST;
        if (kind == KIND_BOOST) {
            // Boost vai no primeiro slot livre; com os 3 ocupados, troca o primeiro.
            for (int b = EQUIP_BOOST; b < EQUIP_SLOTS; b++)
                if (!pr->equipped[b]) { e = b; break; }
        }
        pr->equipped[e] = index + 1;
    }
    update_stats(p);
    send_inventory(slot);
}

static void discard(int slot, uint8_t index) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (index >= BAG_SIZE || pr->bag[index].base == ITEM_NONE) return;
    uint8_t current = equipped_slot(pr, index);
    if (current) pr->equipped[current - 1] = 0;
    pr->bag[index] = (item_t){0};
    pr->enchant[index] = 0; // o encanto vai junto com o item
    update_stats(p);
    send_inventory(slot);
}

static void find_spawn(player_t *p) {
    for (int i = 0; i < 200; i++) {
        int dx = (int)rnd_below(31) - 15, dy = (int)rnd_below(31) - 15;
        int d2 = dx * dx + dy * dy;
        int min = PORT_RADIUS + 2, max = PORT_RADIUS + 8;
        if (d2 < min * min || d2 > max * max) continue;
        if (!tile_is_water(tile(PORT_X + dx, PORT_Y + dy))) continue;
        p->x = PORT_X + dx;
        p->y = PORT_Y + dy;
        return;
    }
    p->x = DOCK_X;
    p->y = DOCK_Y + 1;
}

static void add_fish(int slot, uint8_t species, int n) {
    player_t *p = &g.players[slot];
    int room = p->capacity - hold_count(p);
    if (room <= 0) {
        if (g.tick % 50 == 0) send_event(slot, EV_HOLD_FULL, FISH_NONE, 0, 0);
        return;
    }
    int got = n < room ? n : room;
    prof(p)->hold[species] += got;
    send_event(slot, EV_CATCH, species, (uint16_t)got, 0);
    int luck = p->stats[STAT_LUCK] > 0 ? p->stats[STAT_LUCK] : 0;
    if (chance_permille(CHEST_PER_MILLE + (uint32_t)luck)) give_random_item(slot);
}

static void sell(int slot, uint8_t species) {
    player_t *p = &g.players[slot];
    if (!at_dock(p)) return;
    profile_t *pr = prof(p);
    uint32_t total = 0, count = 0;
    for (int i = 0; i < FISH_COUNT; i++) {
        if (species != FISH_NONE && species != i) continue;
        uint32_t n = pr->hold[i];
        if (!n) continue;
        total += price_cents(i) * n;
        g.pressure[i] += n * 1000;
        count += n;
        pr->hold[i] = 0;
    }
    if (!count) { send_event(slot, EV_NOTHING_TO_SELL, species, 0, 0); return; }
    pr->money += total;
    send_event(slot, EV_SOLD, species, (uint16_t)count, total);
    send_prices(-1);
    send_rank(-1);
}

static const int8_t DIR_X[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int8_t DIR_Y[8] = {0, 1, 1, 1, 0, -1, -1, -1};

static int dir_index(int dx, int dy) {
    for (int i = 0; i < 8; i++)
        if (DIR_X[i] == dx && DIR_Y[i] == dy) return i;
    return -1; // parado
}

// Leme: a proa gira 45° por vez até apontar para `want`, pelo lado mais
// curto. Leme melhor = menos ticks entre uma virada e outra.
static void steer(player_t *p, int want) {
    if (p->vel == 0) p->heading = (uint8_t)want; // parado, vira na hora
    if (want == p->heading) { p->turn_timer = 0; return; }
    if (++p->turn_timer < RUDDER_TURN_TICKS[level(prof(p), PART_RUDDER)]) return;
    p->turn_timer = 0;
    int diff = (want - p->heading + 8) % 8; // passos no sentido horário
    p->heading = (uint8_t)((p->heading + (diff <= 4 ? 1 : 7)) % 8);
}

static void upgrade(int slot, uint8_t part) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (!at_dock(p) || part >= PART_COUNT || level(pr, part) >= BOAT_LEVELS - 1) return;
    int next = level(pr, part) + 1;
    uint32_t price = UPGRADE_PRICE[next];
    if (pr->money < price) { send_event(slot, EV_NO_MONEY, FISH_NONE, 0, price); return; }
    pr->money -= price;
    pr->boat[part] = (uint8_t)next;
    update_stats(p);
    storage_save(p->profile, pr); // dinheiro gasto: salva já
    send_event(slot, EV_UPGRADED, part, (uint16_t)next, price);
    send_boat(slot);
    send_inventory(slot); // os stats do barco mudaram
    send_rank(-1);
}

static void repair(int slot) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (!at_dock(p)) return;
    int missing = hp_max(p) - hp(p);
    if (missing <= 0) return;
    uint32_t price = (uint32_t)missing * REPAIR_CENTS_PER_HP;
    if (pr->money < price) { send_event(slot, EV_NO_MONEY, FISH_NONE, 0, price); return; }
    pr->money -= price;
    pr->damage = 0;
    update_stats(p); // sai da avaria
    storage_save(p->profile, pr);
    send_event(slot, EV_REPAIRED, FISH_NONE, (uint16_t)missing, price);
    send_rank(-1);
}

// Mercenário: fica com as bags dos outros e passa os peixes delas para o
// seu porão, até encher. Por coroa ele paga uma recompensa, menos pela do
// alvo da sereia: essa é ela quem quer.
static void mercenary(int slot) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (!at_dock(p)) return;
    int got = 0, crowns = 0, left = 0;
    for (int i = 0; i < LOOT_SLOTS; i++) {
        loot_t *l = &pr->loot[i];
        if (l->kind == LOOT_CROWN) {
            if (pr->has_mission && l->owner == pr->mission_target) continue;
            crowns++;
            *l = (loot_t){0};
        } else if (l->kind == LOOT_BAG) {
            int rest = 0;
            for (int f = 0; f < FISH_COUNT; f++) {
                int room = p->capacity - hold_count(p);
                int n = room <= 0 ? 0 : l->fish[f] < room ? l->fish[f] : room;
                pr->hold[f] += n;
                l->fish[f] -= n;
                got += n;
                rest += l->fish[f];
            }
            if (rest) left = 1; // porão cheio: a bag fica com o resto
            else *l = (loot_t){0};
        }
    }
    if (!got && !crowns) { send_event(slot, EV_MERC_NOTHING, (uint8_t)left, 0, 0); return; }
    uint32_t bounty = (uint32_t)crowns * CROWN_BOUNTY_CENTS;
    pr->money += bounty;
    storage_save(p->profile, pr);
    send_event(slot, EV_MERC, (uint8_t)left, (uint16_t)got, bounty);
    send_inventory(slot);
    if (bounty) send_rank(-1);
}

// Roleta da sereia: pode roubar seu dinheiro, dar a missão de afundar
// alguém ou só cantar.
static void spin(int slot) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (g.tick < p->next_spin) {
        send_event(slot, EV_SPIN_WAIT, FISH_NONE, 0, (p->next_spin - g.tick + TICK_HZ - 1) / TICK_HZ);
        return;
    }
    p->next_spin = g.tick + SPIN_COOLDOWN;
    uint32_t r = rnd_below(100);
    if (r < 40) {
        uint32_t stolen = pr->money * STEAL_PCT / 100;
        if (stolen > STEAL_MAX_CENTS) stolen = STEAL_MAX_CENTS;
        pr->money -= stolen;
        storage_save(p->profile, pr);
        send_event(slot, EV_SPIN_STEAL, FISH_NONE, 0, stolen);
        if (stolen) send_rank(-1);
        return;
    }
    if (r < 85) {
        // Alvo: alguém online, se tiver. Senão, qualquer capitão já criado.
        int online[MAX_PLAYERS], n_online = 0, n_any = 0;
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (i != slot && g.players[i].state == SLOT_PLAYING) online[n_online++] = i;
        for (int i = 0; i < MAX_PROFILES; i++) n_any += g.profiles[i].used && i != p->profile;
        int target = -1;
        if (n_online) {
            int hunted = online[rnd_below((uint32_t)n_online)];
            target = g.players[hunted].profile;
            send_event(hunted, EV_HUNTED, (uint8_t)slot, 0, 0);
        } else if (n_any) {
            int k = (int)rnd_below((uint32_t)n_any);
            for (int i = 0; i < MAX_PROFILES && target < 0; i++)
                if (g.profiles[i].used && i != p->profile && k-- == 0) target = i;
        }
        if (target >= 0) {
            pr->has_mission = 1;
            pr->mission_target = (uint8_t)target;
            storage_save(p->profile, pr);
            send_inventory(slot); // leva o nome do alvo
            send_event(slot, EV_SPIN_MISSION, FISH_NONE, 0, 0);
            return;
        }
    }
    send_event(slot, EV_SPIN_NOTHING, FISH_NONE, 0, 0);
}

// Entregar a coroa do alvo: a sereia encanta um item aleatório da bolsa.
static void deliver_crown(int slot) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    int crown = -1;
    for (int i = 0; i < LOOT_SLOTS && crown < 0; i++)
        if (pr->loot[i].kind == LOOT_CROWN && pr->loot[i].owner == pr->mission_target) crown = i;
    if (crown < 0) { send_event(slot, EV_MISSION_WAIT, FISH_NONE, 0, 0); return; }

    int options[BAG_SIZE], n = 0;
    for (int i = 0; i < BAG_SIZE; i++)
        if (pr->bag[i].base != ITEM_NONE && pr->enchant[i] < ENCHANT_MAX) options[n++] = i;
    if (!n) { send_event(slot, EV_NOTHING_TO_ENCHANT, FISH_NONE, 0, 0); return; }

    int pick = options[rnd_below((uint32_t)n)];
    pr->enchant[pick]++;
    pr->loot[crown] = (loot_t){0};
    pr->has_mission = 0;
    update_stats(p); // se o item estava equipado, o barco já sente
    storage_save(p->profile, pr);
    send_event(slot, EV_ENCHANT, pr->bag[pick].base, pr->enchant[pick], (uint32_t)pick);
    send_inventory(slot);
}

static void mermaid(int slot, uint8_t action) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (!near_mermaid(p)) return;
    if (action == MERMAID_GIVE_UP) {
        if (!pr->has_mission) return;
        pr->has_mission = 0;
        storage_save(p->profile, pr);
        send_event(slot, EV_MISSION_GIVE_UP, FISH_NONE, 0, 0);
        send_inventory(slot);
    } else if (pr->has_mission) {
        deliver_crown(slot);
    } else {
        spin(slot);
    }
}

// Mecânico: dois itens do mesmo tipo viram um só, sorteado de novo pelo
// gerador. Sai no mínimo com a raridade do melhor dos dois; se os dois têm a
// mesma raridade, sai uma acima. O encanto maior fica.
static void combine(int slot, uint8_t a, uint8_t b) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (!at_dock(p) || a >= BAG_SIZE || b >= BAG_SIZE || a == b) return;
    item_t ia = pr->bag[a], ib = pr->bag[b];
    if (ia.base == ITEM_NONE || ia.base != ib.base) { send_event(slot, EV_CANT_COMBINE, FISH_NONE, 0, 0); return; }

    uint8_t ra = item_info(ia).rarity, rb = item_info(ib).rarity;
    uint8_t want = ra > rb ? ra : rb;
    if (ra == rb && want < RARITY_LEGENDARY) want++;
    uint32_t price = COMBINE_PRICE[want];
    if (pr->money < price) { send_event(slot, EV_NO_MONEY, FISH_NONE, 0, price); return; }

    // Sorteia sementes até sair a raridade pedida (lendário é 1 em 100).
    item_t made = item_new(ia.base, rnd());
    for (int tries = 0; tries < 100000 && item_info(made).rarity < want; tries++) made = item_new(ia.base, rnd());

    uint8_t ea = equipped_slot(pr, a), eb = equipped_slot(pr, b);
    if (eb) pr->equipped[eb - 1] = ea ? 0 : a + 1; // estava equipado: o novo fica no lugar
    pr->enchant[a] = pr->enchant[a] > pr->enchant[b] ? pr->enchant[a] : pr->enchant[b];
    pr->bag[a] = made;
    pr->bag[b] = (item_t){0};
    pr->enchant[b] = 0;
    pr->money -= price;
    update_stats(p);
    storage_save(p->profile, pr);
    send_event(slot, EV_COMBINED, made.base, item_info(made).rarity, price);
    send_inventory(slot);
    send_rank(-1);
}

static int try_move(player_t *p) {
    int dx = DIR_X[p->heading], dy = DIR_Y[p->heading];
    int nx = p->x + dx, ny = p->y + dy;
    if (tile_is_sailable(tile(nx, ny))) {
        p->x = nx;
        p->y = ny;
        return 1;
    }
    // Na diagonal, desliza pela costa.
    if (dx && dy && tile_is_sailable(tile(nx, p->y))) return p->x = nx, 1;
    if (dx && dy && tile_is_sailable(tile(p->x, ny))) return p->y = ny, 1;
    return 0;
}

static void crash(int slot) {
    player_t *p = &g.players[slot];
    int hit = p->vel >= CRASH_MIN_SPEED && !p->blocked && !at_dock(p);
    int dmg = p->vel * CRASH_DAMAGE;
    p->vel = 0;
    p->move_budget = 0;
    p->blocked = 1;
    if (!hit || hp(p) == 0) return;
    profile_t *pr = prof(p);
    pr->damage = (uint16_t)(pr->damage + dmg > hp_max(p) ? hp_max(p) : pr->damage + dmg);
    update_stats(p);
    send_event(slot, EV_CRASH, FISH_NONE, (uint16_t)dmg, (uint32_t)hp(p));
    if (hp(p) == 0) send_event(slot, EV_BROKEN, FISH_NONE, 0, 0);
}

// A arma equipada, ou NULL. O dano vem do item (raridade e afixos contam).
static const weapon_t *weapon(const player_t *p) {
    const profile_t *pr = prof(p);
    if (!pr->equipped[EQUIP_WEAPON]) return 0;
    return weapon_of(pr->bag[pr->equipped[EQUIP_WEAPON] - 1].base);
}

static void fire(int slot, int tx, int ty) {
    player_t *p = &g.players[slot];
    const weapon_t *wp = weapon(p);
    if (!wp || p->reload) return;
    if (in_safe_zone(p->x, p->y)) { send_event(slot, EV_SAFE_ZONE, FISH_NONE, 0, 0); return; }
    int dx = tx - p->x, dy = ty - p->y;
    uint32_t len = isqrt((uint32_t)(dx * dx + dy * dy));
    if (len == 0) return;
    for (int i = 0; i < MAX_SHOTS; i++) {
        shot_t *sh = &g.shots[i];
        if (sh->alive) continue;
        *sh = (shot_t){
            .alive = 1,
            .owner = (uint8_t)slot,
            .speed = wp->speed,
            .left = wp->range,
            .x = p->x * SUB,
            .y = p->y * SUB,
            .ux = dx * SUB / (int32_t)len,
            .uy = dy * SUB / (int32_t)len,
            .damage = (uint16_t)(p->stats[STAT_DAMAGE] > 1 ? p->stats[STAT_DAMAGE] : 1),
        };
        p->reload = wp->reload;
        return;
    }
}

// Joga um saque no mar, numa célula de água perto de (x, y). Com o mar
// cheio de saques, o mais velho dá lugar ao novo.
static void drop_loot(int x, int y, const loot_t *loot) {
    int pick = 0;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (!g.drops[i].alive) { pick = i; break; }
        if (g.drops[i].expires < g.drops[pick].expires) pick = i;
    }
    for (int tries = 0; tries < 20; tries++) {
        int nx = x + (tries ? (int)rnd_below(5) - 2 : 0), ny = y + (tries ? (int)rnd_below(5) - 2 : 0);
        if (!tile_is_water(tile(nx, ny))) continue;
        x = nx, y = ny;
        break;
    }
    g.drops[pick] = (drop_t){.alive = 1, .x = (uint16_t)x, .y = (uint16_t)y, .expires = g.tick + DROP_LIFE, .loot = *loot};
}

// Morte por tiro: o porão vira uma bag, cai a coroa, cai o saque que o barco
// carregava e o jogador renasce no porto com o barco inteiro.
static void die(int slot, int killer) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    loot_t bag = {.kind = LOOT_BAG, .owner = (uint8_t)p->profile};
    int fish = 0;
    for (int i = 0; i < FISH_COUNT; i++) fish += bag.fish[i] = pr->hold[i];
    memset(pr->hold, 0, sizeof pr->hold);
    if (fish) drop_loot(p->x, p->y, &bag);
    drop_loot(p->x, p->y, &(loot_t){.kind = LOOT_CROWN, .owner = (uint8_t)p->profile});
    for (int i = 0; i < LOOT_SLOTS; i++) {
        if (pr->loot[i].kind != LOOT_NONE) drop_loot(p->x, p->y, &pr->loot[i]);
        pr->loot[i] = (loot_t){0};
    }

    pr->deaths++;
    pr->damage = 0;
    p->vel = 0;
    p->move_budget = 0;
    find_spawn(p);
    update_stats(p);
    storage_save(p->profile, pr);
    send_event(slot, EV_DIED, (uint8_t)killer, (uint16_t)fish, 0);
    send_inventory(slot);

    if (killer >= 0 && g.players[killer].state == SLOT_PLAYING) {
        prof(&g.players[killer])->kills++;
        send_event(killer, EV_KILL, (uint8_t)slot, 0, 0);
    }
    send_marks(-1);
}

static void hit(int target, shot_t *sh) {
    player_t *t = &g.players[target];
    profile_t *pr = prof(t);
    int dmg = sh->damage;
    int shooter = g.players[sh->owner].state == SLOT_PLAYING ? sh->owner : -1;
    pr->damage = (uint16_t)(pr->damage + dmg > hp_max(t) ? hp_max(t) : pr->damage + dmg);
    send_event(target, EV_HURT, sh->owner, (uint16_t)dmg, (uint32_t)hp(t));
    if (shooter >= 0) send_event(shooter, EV_HIT, (uint8_t)target, (uint16_t)dmg, (uint32_t)hp(t));
    // Vida 0 por tiro é morte (na costa continua sendo só avaria).
    if (hp(t) == 0) die(target, shooter);
}

// Passou por cima de uma bag ou coroa. A sua volta para você (os peixes
// voltam ao porão); a dos outros fica guardada no barco.
static void pick_up(int slot) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    for (int i = 0; i < MAX_DROPS; i++) {
        drop_t *d = &g.drops[i];
        if (!d->alive || d->x < p->x - 1 || d->x > p->x + 1 || d->y < p->y - 1 || d->y > p->y + 1) continue;
        if (d->loot.owner == p->profile) {
            int fish = 0;
            for (int f = 0; f < FISH_COUNT; f++) fish += d->loot.fish[f];
            if (fish > p->capacity - hold_count(p)) {
                if (g.tick % 50 == 0) send_event(slot, EV_HOLD_FULL, FISH_NONE, 0, 0);
                continue;
            }
            for (int f = 0; f < FISH_COUNT; f++) pr->hold[f] += d->loot.fish[f];
        } else {
            int free_slot = -1;
            for (int l = 0; l < LOOT_SLOTS && free_slot < 0; l++)
                if (pr->loot[l].kind == LOOT_NONE) free_slot = l;
            if (free_slot < 0) {
                if (g.tick % 50 == 0) send_event(slot, EV_LOOT_FULL, FISH_NONE, 0, 0);
                continue;
            }
            pr->loot[free_slot] = d->loot;
        }
        d->alive = 0;
        send_event(slot, EV_LOOT, d->loot.kind, d->loot.owner == p->profile, 0); // count 1 = era seu
        send_inventory(slot);
        send_marks(-1);
    }
}

static void update_drops(void) {
    int changed = 0;
    for (int i = 0; i < MAX_DROPS; i++) {
        if (g.drops[i].alive && g.drops[i].expires <= g.tick) g.drops[i].alive = 0, changed = 1;
    }
    if (changed) send_marks(-1);
}

// O tiro anda uma célula por vez (até `speed` por tick), para não pular
// por cima de um barco. Some ao bater num barco, na terra ou no fim do alcance.
static void update_shots(void) {
    for (int i = 0; i < MAX_SHOTS; i++) {
        shot_t *sh = &g.shots[i];
        for (int step = 0; sh->alive && step < sh->speed; step++) {
            sh->x += sh->ux;
            sh->y += sh->uy;
            int cx = shot_cell(sh->x), cy = shot_cell(sh->y);
            if (!sh->left-- || !tile_is_sailable(tile(cx, cy))) { sh->alive = 0; break; }
            for (int t = 0; t < MAX_PLAYERS; t++) {
                const player_t *o = &g.players[t];
                // O barco ocupa 3 células na largura (o desenho tem até 7).
                if (t == sh->owner || o->state != SLOT_PLAYING || o->y != cy || o->x < cx - 1 || o->x > cx + 1) continue;
                if (!in_safe_zone(o->x, o->y)) hit(t, sh);
                sh->alive = 0;
                break;
            }
        }
    }
}

static void update_player(int slot) {
    player_t *p = &g.players[slot];
    // Inércia: com tecla apertada acelera 1 célula/s por tick; soltou,
    // o barco desliza e perde 1 célula/s a cada 2 ticks.
    int want = dir_index(p->dx, p->dy);
    if (want >= 0) {
        steer(p, want);
        if (p->vel < p->speed) p->vel++;
    } else if (p->vel > 0 && g.tick % 2 == 0) {
        p->vel--;
    }
    if (p->vel > p->speed) p->vel = p->speed; // tirou o motor, por exemplo
    if (p->reload) p->reload--;
    int moved = 0;
    p->move_budget += p->vel; // vel células/s = vel décimos por tick
    while (p->move_budget >= 10) {
        p->move_budget -= 10;
        if (!try_move(p)) {
            crash(slot);
            break;
        }
        moved = 1;
        p->blocked = 0;
    }
    if (p->vel == 0) p->move_budget = 0;
    prof(p)->x = p->x;
    prof(p)->y = p->y;
    pick_up(slot);

    // Produção = % a mais na chance de pescar. +50 de produção: 6% vira 9%.
    int production = p->stats[STAT_PRODUCTION] > -100 ? p->stats[STAT_PRODUCTION] : -100;
    uint32_t passive = (uint32_t)(PASSIVE_CATCH_PCT * 10 * (100 + production) / 100);
    uint32_t school = (uint32_t)(SCHOOL_CATCH_PCT * 10 * (100 + production) / 100);
    if (moved && chance_permille(passive)) {
        uint8_t sp = fish_roll(tile(p->x, p->y), rnd());
        if (sp != FISH_NONE) add_fish(slot, sp, 1);
    }
    for (int i = 0; i < MAX_SCHOOLS; i++) {
        const school_t *s = &g.schools[i];
        int dx = p->x - s->x, dy = p->y - s->y;
        if (s->alive && dx * dx + dy * dy <= s->r * s->r && chance_permille(school))
            add_fish(slot, s->species, 1 + (int)rnd_below(3));
    }
}

static void update_schools(void) {
    int attempts = 2000;
    for (int i = 0; i < MAX_SCHOOLS; i++) {
        school_t *s = &g.schools[i];
        if (s->alive && s->expires <= g.tick) s->alive = 0;
        // Nada de cardume colado no porto: precisa sair para buscar.
        while (!s->alive && attempts-- > 0) {
            int x = (int)rnd_below(MAP_W), y = (int)rnd_below(MAP_H);
            int dx = x - PORT_X, dy = y - PORT_Y;
            tile_t t = tile(x, y);
            if (!tile_is_water(t) || dx * dx + dy * dy < 30 * 30) continue;
            *s = (school_t){
                .alive = 1,
                .x = (uint16_t)x,
                .y = (uint16_t)y,
                .r = (uint8_t)(2 + rnd_below(4)),
                .species = fish_roll(t, rnd()),
                .expires = g.tick + 600 + rnd_below(1200), // 1 a 3 minutos
            };
        }
    }
    // Cardumes migram devagar.
    if (g.tick % 20 != 0) return;
    for (int i = 0; i < MAX_SCHOOLS; i++) {
        school_t *s = &g.schools[i];
        int nx = s->x + (int)rnd_below(3) - 1, ny = s->y + (int)rnd_below(3) - 1;
        if (s->alive && tile_is_water(tile(nx, ny))) {
            s->x = (uint16_t)nx;
            s->y = (uint16_t)ny;
        }
    }
}

static int valid_name(const uint8_t *name, size_t len) {
    if (len < 1 || len > NAME_MAX_LEN) return 0;
    int letters = 0;
    for (size_t i = 0; i < len; i++) {
        if (name[i] < 32 || name[i] == 127) return 0;
        letters += name[i] != ' ';
    }
    return letters > 0;
}

static int valid_password(const uint8_t *pass, size_t len) {
    if (len < PASSWORD_MIN_LEN || len > PASSWORD_MAX_LEN) return 0;
    for (size_t i = 0; i < len; i++)
        if (pass[i] < 32 || pass[i] > 126) return 0;
    return 1;
}

static void join(int slot, const uint8_t *data, size_t len) {
    if (len < 2 || data[0] != PROTOCOL_VERSION) { send_error(slot, ERR_VERSION); return; }
    size_t name_len = data[1];
    if (len < 2 + name_len || !valid_name(data + 2, name_len)) { send_error(slot, ERR_BAD_NAME); return; }
    char name[NAME_MAX_LEN + 1];
    memcpy(name, data + 2, name_len);
    name[name_len] = 0;

    const uint8_t *pass = data + 2 + name_len + 1;
    size_t pass_len = len > 2 + name_len ? data[2 + name_len] : 0;
    if (len < 3 + name_len + pass_len || !valid_password(pass, pass_len)) {
        send_error(slot, ERR_BAD_PASSWORD);
        return;
    }

    int index = -1, free_index = -1;
    for (int i = 0; i < MAX_PROFILES; i++) {
        if (!g.profiles[i].used) {
            if (free_index < 0) free_index = i;
        } else if (same_name(g.profiles[i].name, name)) {
            index = i;
            break;
        }
    }
    uint8_t hash[PASSWORD_HASH_LEN];
    if (index >= 0) {
        // Perfil existe: a senha precisa bater. Confere antes de dizer se
        // está online, para quem não sabe a senha não descobrir nada.
        profile_t *pr = &g.profiles[index];
        if (!password_hash(pass, pass_len, pr->salt, hash) || !password_equal(hash, pr->pass_hash)) {
            send_error(slot, ERR_WRONG_PASSWORD);
            return;
        }
        if (profile_online(index)) { send_error(slot, ERR_NAME_ONLINE); return; }
    } else {
        // Nome novo: cria o perfil com esta senha.
        if (free_index < 0) { send_error(slot, ERR_FULL); return; }
        profile_t pr = {.used = 1};
        memcpy(pr.name, name, name_len + 1);
        password_random(pr.salt, PASSWORD_SALT_LEN);
        if (!password_hash(pass, pass_len, pr.salt, pr.pass_hash)) { send_error(slot, ERR_BAD_PASSWORD); return; }
        item_t rod;
        do rod = item_new(ITEM_VARA, rnd());
        while (item_info(rod).rarity != RARITY_COMMON);
        pr.bag[0] = rod;
        pr.equipped[EQUIP_TOOL] = 1;
        index = free_index;
        g.profiles[index] = pr;
        storage_save(index, &g.profiles[index]);
    }
    player_t *p = &g.players[slot];
    *p = (player_t){
        .state = SLOT_PLAYING,
        .profile = index,
        .color = (uint8_t)rnd_below(6),
    };
    update_stats(p);
    // Volta para onde parou, se ainda for água; senão nasce perto do porto.
    const profile_t *pr = &g.profiles[index];
    if ((pr->x || pr->y) && tile_is_sailable(tile(pr->x, pr->y))) {
        p->x = pr->x;
        p->y = pr->y;
    } else {
        find_spawn(p);
    }

    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_WELCOME);
    put_u8(&w, (uint8_t)slot);
    put_u32(&w, g.seed);
    put_u16(&w, MAP_W);
    put_u16(&w, MAP_H);
    put_u32(&w, g.checksum);
    put_u8(&w, TICK_HZ);
    send_w(slot, &w);

    // Apresenta todo mundo para o novo jogador e o novo jogador para todo mundo.
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g.players[i].state != SLOT_PLAYING) continue;
        w = writer(out, sizeof out);
        write_player_info(&w, i);
        send_w(slot, &w);
        if (i != slot) {
            w = writer(out, sizeof out);
            write_player_info(&w, slot);
            send_w(i, &w);
        }
    }
    send_prices(slot);
    send_marks(slot);
    send_inventory(slot);
    send_boat(slot);
    send_rank(-1);
    send_event(slot, EV_WELCOME, FISH_NONE, 0, 0);
}

// A pedra da sereia: sempre no mesmo lugar para o mesmo mapa (sai da semente
// do mapa, não do sorteio do servidor). Água em volta para dar para chegar.
static void place_mermaid(void) {
    safe_zone_t *z = &g.safe_zones[ZONE_MERMAID];
    *z = (safe_zone_t){PORT_X + 16, PORT_Y, MERMAID_SAFE_R}; // se não achar lugar
    for (int i = 0; i < 5000; i++) {
        uint32_t h = world_hash(i, 0x5E7EA, g.seed);
        int dx = (int)(h % (2 * MERMAID_MAX_DIST + 1)) - MERMAID_MAX_DIST;
        int dy = (int)((h >> 16) % (2 * MERMAID_MAX_DIST + 1)) - MERMAID_MAX_DIST;
        int d2 = dx * dx + dy * dy;
        if (d2 < MERMAID_MIN_DIST * MERMAID_MIN_DIST || d2 > MERMAID_MAX_DIST * MERMAID_MAX_DIST) continue;
        int water = 1;
        for (int y = -2; y <= 2 && water; y++)
            for (int x = -2; x <= 2 && water; x++) water = tile_is_water(tile(PORT_X + dx + x, PORT_Y + dy + y));
        if (!water) continue;
        z->x = (int16_t)(PORT_X + dx);
        z->y = (int16_t)(PORT_Y + dy);
        return;
    }
}

// --- API --------------------------------------------------------------------

void game_init(uint8_t *tiles, uint32_t seed, uint32_t random_seed, game_send_fn send) {
    memset(&g, 0, sizeof g);
    g.tiles = tiles;
    g.seed = seed;
    g.rng = random_seed ? random_seed : 0x9E3779B9u;
    g.send = send;
    world_generate(tiles, seed);
    g.checksum = world_checksum(tiles);
    g.safe_zones[ZONE_PORT] = (safe_zone_t){PORT_X, PORT_Y, PORT_RADIUS + 14};
    place_mermaid();
    storage_load_all(g.profiles, MAX_PROFILES);
    update_schools();
}

int game_connect(void) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (g.players[i].state == SLOT_FREE) {
            g.players[i] = (player_t){.state = SLOT_CONNECTED};
            return i;
        }
    }
    return -1;
}

void game_disconnect(int slot) {
    if (slot < 0 || slot >= MAX_PLAYERS) return;
    player_t *p = &g.players[slot];
    int was_playing = p->state == SLOT_PLAYING;
    if (was_playing) storage_save(p->profile, prof(p));
    p->state = SLOT_FREE;
    if (was_playing) send_rank(-1);
}

void game_receive(int slot, const uint8_t *data, size_t len) {
    if (slot < 0 || slot >= MAX_PLAYERS || len < 1) return;
    player_t *p = &g.players[slot];
    if (p->state == SLOT_CONNECTED) {
        if (data[0] == C_JOIN) join(slot, data + 1, len - 1);
        return;
    }
    if (p->state != SLOT_PLAYING) return;
    switch (data[0]) {
    case C_INPUT:
        if (len < 3) return;
        p->dx = (int8_t)data[1] > 0 ? 1 : (int8_t)data[1] < 0 ? -1 : 0;
        p->dy = (int8_t)data[2] > 0 ? 1 : (int8_t)data[2] < 0 ? -1 : 0;
        break;
    case C_SELL:
        if (len < 2 || (data[1] >= FISH_COUNT && data[1] != FISH_NONE)) return;
        sell(slot, data[1]);
        break;
    case C_EQUIP:
        if (len >= 2) equip(slot, data[1]);
        break;
    case C_DISCARD:
        if (len >= 2) discard(slot, data[1]);
        break;
    case C_UPGRADE:
        if (len >= 2) upgrade(slot, data[1]);
        break;
    case C_REPAIR:
        repair(slot);
        break;
    case C_FIRE:
        if (len >= 5) fire(slot, data[1] | data[2] << 8, data[3] | data[4] << 8);
        break;
    case C_MERC:
        mercenary(slot);
        break;
    case C_MERMAID:
        if (len >= 2) mermaid(slot, data[1]);
        break;
    case C_COMBINE:
        if (len >= 3) combine(slot, data[1], data[2]);
        break;
    }
}

void game_save_all(void) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) storage_save(g.players[i].profile, prof(&g.players[i]));
}

void game_tick(void) {
    g.tick++;
    update_schools();
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) update_player(i);
    
    update_shots();
    update_drops();
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) send_state(i);

    if (g.tick % TICK_HZ == 0) {
        for (int i = 0; i < FISH_COUNT; i++) g.pressure[i] = g.pressure[i] * 99 / 100;
        send_prices(-1);
        send_marks(-1);
    }
    if (g.tick % 50 == 0) send_rank(-1);
    if (g.tick % SAVE_EVERY == 0) game_save_all();
}
