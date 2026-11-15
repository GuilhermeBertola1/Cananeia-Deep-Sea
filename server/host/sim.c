// Simulação sem rede: o núcleo do jogo (game.c) roda direto, sem WebSocket e
// sem esperar o relógio. Uma hora de jogo passa em segundos.
//
// Dois tipos de barco no mar:
//   - capitães de teste, iguais aos robôs do bots.c: fazem papel de gente e
//     falam com o jogo pelas mesmas mensagens do cliente (C_JOIN, C_INPUT,
//     C_SELL), só que chamando game_receive() em vez de mandar pela rede;
//   - pescadores NPC, que moram dentro do jogo (game_add_npcs) e aprendem
//     qual estratégia rende mais com o cérebro (brain.c).
//
// A cada minuto de jogo sai uma linha no CSV com a economia (dinheiro, peixes
// vendidos, preços no porto e a média dos balcões) e o que os NPCs estão
// fazendo. Mesma semente = mesmo resultado, sempre: é isso que deixa comparar
// uma mudança com a outra.
//
// Uso: sim [capitães] [minutos] [semente] [arquivo.csv] [npcs]
//   sim                     -> 8 capitães, 60 min, semente 1337, sim.csv, 8 NPCs
//   sim 4 240 42 a.csv 16   -> 4 capitães, 4 horas, semente 42, 16 NPCs
//   sim 8 60 1337 b.csv 0   -> sem NPC nenhum (igual ao passo 1)
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "game.h"
#include "protocol.h"

#define PASSWORD "capitao123"
#define WANDER_RANGE 80 // o capitão pesca a até 80 células do cais
#define FULL_PCT 80     // com o porão 80% cheio, volta para vender

typedef enum { GO_FISH = 0, GO_HOME } plan_t;

typedef struct {
    int slot;     // -1 = não entrou
    int playing;  // recebeu S_WELCOME
    int x, y;
    uint32_t money;
    int capacity, hold, hp, hp_max;
    uint8_t flags;
    plan_t plan;
    int8_t dx, dy;          // rumo que está mandando
    uint32_t next_think;    // tick da próxima decisão
    uint32_t detour_until;  // voltando: desvia até este tick (bateu na costa)
    int last_x, last_y;     // onde estava na última decisão
} captain_t;

static captain_t caps[HUMAN_SLOTS];
static int n_caps = 8;
static int by_slot[MAX_PLAYERS]; // slot -> capitão (-1 = não é capitão)
static const char *STRATEGY[NPC_STRATEGIES] = {"porto", "cardume", "estoque", "vila"};
static unsigned errors;
static uint32_t tick; // nosso relógio: o jogo não usa o relógio de verdade

static uint32_t rng;
static uint32_t rnd(void) {
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return rng;
}

static int sign(int v) { return v > 0 ? 1 : v < 0 ? -1 : 0; }

// --- o que o jogo "manda pela rede" ------------------------------------------

static uint16_t u16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t u32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

// O jogo chama isto em vez de mandar bytes pelo WebSocket. O capitão só
// precisa do S_STATE (onde está, porão, vida) e de saber se deu erro.
static void on_send(int slot, const uint8_t *m, size_t len) {
    if (slot < 0 || slot >= MAX_PLAYERS || by_slot[slot] < 0 || len < 1) return;
    captain_t *c = &caps[by_slot[slot]];
    switch (m[0]) {
    case S_WELCOME:
        c->playing = 1;
        break;
    case S_ERROR:
        errors++;
        fprintf(stderr, "capitão %d recebeu S_ERROR %d\n", by_slot[slot] + 1, len > 1 ? m[1] : -1);
        break;
    case S_STATE:
        // u8 op, u32 tick, u16 x, u16 y, u32 dinheiro, u16 capacidade, u8 speed,
        // u8 vel, u8 flags, u16 vida, u16 vida máx, u8 recarga, u16 porão[FISH_COUNT]
        if (len < 23 + 2 * FISH_COUNT) return;
        c->x = u16(m + 5), c->y = u16(m + 7);
        c->money = u32(m + 9);
        c->capacity = u16(m + 13);
        c->flags = m[17];
        c->hp = u16(m + 18), c->hp_max = u16(m + 20);
        c->hold = 0;
        for (int i = 0; i < FISH_COUNT; i++) c->hold += u16(m + 23 + 2 * i);
        break;
    }
}

// --- o capitão ----------------------------------------------------------------

static void send_msg(captain_t *c, const uint8_t *m, size_t len) { game_receive(c->slot, m, len); }

static void steer(captain_t *c, int dx, int dy) {
    if (dx == c->dx && dy == c->dy) return; // o cliente também só manda quando muda
    c->dx = (int8_t)dx, c->dy = (int8_t)dy;
    uint8_t m[3] = {C_INPUT, (uint8_t)c->dx, (uint8_t)c->dy};
    send_msg(c, m, 3);
}

static void random_heading(captain_t *c) {
    int dx, dy;
    do dx = (int)(rnd() % 3) - 1, dy = (int)(rnd() % 3) - 1;
    while (!dx && !dy);
    steer(c, dx, dy);
}

static void join(int i) {
    captain_t *c = &caps[i];
    c->slot = game_connect();
    if (c->slot < 0) return;
    by_slot[c->slot] = i;
    char name[24];
    snprintf(name, sizeof name, "capitao%d", i + 1);
    uint8_t m[64];
    size_t nl = strlen(name), pl = strlen(PASSWORD), p = 0;
    m[p++] = C_JOIN;
    m[p++] = PROTOCOL_VERSION;
    m[p++] = (uint8_t)nl;
    memcpy(m + p, name, nl), p += nl;
    m[p++] = (uint8_t)pl;
    memcpy(m + p, PASSWORD, pl), p += pl;
    send_msg(c, m, p);
}

// Decide uma vez por segundo, mais ou menos (como um jogador apertando teclas).
static void think(captain_t *c) {
    if (!c->playing || tick < c->next_think) return;
    c->next_think = tick + TICK_HZ / 2 + rnd() % TICK_HZ;
    int stuck = c->x == c->last_x && c->y == c->last_y;
    c->last_x = c->x, c->last_y = c->y;

    if (c->flags & STATE_AT_DOCK) {
        // No cais: vende tudo, conserta e sai de novo.
        if (c->hold) {
            uint8_t m[2] = {C_SELL, FISH_NONE};
            send_msg(c, m, 2);
        }
        if (c->hp < c->hp_max) {
            uint8_t m[1] = {C_REPAIR};
            send_msg(c, m, 1);
        }
        c->plan = GO_FISH;
    }

    if (c->plan == GO_FISH) {
        if (c->capacity && c->hold * 100 >= c->capacity * FULL_PCT) {
            c->plan = GO_HOME;
        } else {
            // Pesca andando (a pesca passiva só acontece com o barco andando).
            // Longe demais do cais, vira para ele; senão muda de rumo às vezes.
            int far = abs(c->x - DOCK_X) > WANDER_RANGE || abs(c->y - DOCK_Y) > WANDER_RANGE;
            if (far) steer(c, sign(DOCK_X - c->x), sign(DOCK_Y - c->y));
            else if (stuck || rnd() % 6 == 0) random_heading(c);
            return;
        }
    }

    // Voltando: aponta para o cais. Se bateu na costa, desvia por uns segundos
    // para um lado qualquer e tenta de novo (o pescador NPC do passo 3 vai
    // achar o caminho de verdade).
    if (tick < c->detour_until) {
        if (stuck) random_heading(c);
        return;
    }
    if (stuck && (c->dx || c->dy)) {
        random_heading(c);
        c->detour_until = tick + (2 + rnd() % 4) * TICK_HZ;
        return;
    }
    steer(c, sign(DOCK_X - c->x), sign(DOCK_Y - c->y));
}

// --- relatório ------------------------------------------------------------------

static void csv_header(FILE *f) {
    fprintf(f, "minuto,online,dinheiro,porao,vendidos,ganho,cardumes,predadores,vilas,regioes_pescadas,chunks");
    for (int sp = 0; sp < FISH_COUNT; sp++) fprintf(f, ",porto_%s", SPECIES[sp].name);
    for (int sp = 0; sp < FISH_COUNT; sp++) fprintf(f, ",media_%s", SPECIES[sp].name);
    fprintf(f, ",npcs,npc_dinheiro,npc_viagens,npc_mortes,npc_reboques");
    for (int i = 0; i < NPC_STRATEGIES; i++) fprintf(f, ",usando_%s", STRATEGY[i]);
    for (int i = 0; i < NPC_STRATEGIES; i++) fprintf(f, ",melhor_%s", STRATEGY[i]);
    fprintf(f, "\n");
}

// Dinheiro em reais com 2 casas, para abrir direto numa planilha.
static void csv_row(FILE *f, int minute, const game_stats_t *s) {
    fprintf(f, "%d,%d,%.2f,%u,%u,%.2f,%d,%d,%d,%d,%zu", minute, s->online, s->money / 100.0, s->hold, s->sold,
            s->earned / 100.0, s->schools, s->predators, s->villages, s->stock_regions, s->chunks);
    for (int sp = 0; sp < FISH_COUNT; sp++) fprintf(f, ",%.2f", s->price_port[sp] / 100.0);
    for (int sp = 0; sp < FISH_COUNT; sp++) fprintf(f, ",%.2f", s->price_avg[sp] / 100.0);
    fprintf(f, ",%d,%.2f,%u,%u,%u", s->npcs, s->npc_money / 100.0, s->npc_trips, s->npc_deaths, s->npc_tows);
    for (int i = 0; i < NPC_STRATEGIES; i++) fprintf(f, ",%d", s->npc_using[i]);
    for (int i = 0; i < NPC_STRATEGIES; i++) fprintf(f, ",%d", s->npc_best[i]);
    fprintf(f, "\n");
}

int main(int argc, char **argv) {
    if (argc > 1) n_caps = atoi(argv[1]);
    int minutes = argc > 2 ? atoi(argv[2]) : 60;
    uint32_t seed = argc > 3 ? (uint32_t)strtoul(argv[3], NULL, 10) : 1337;
    const char *path = argc > 4 ? argv[4] : "sim.csv";
    int n_npcs = argc > 5 ? atoi(argv[5]) : 8;
    if (n_caps < 0) n_caps = 0;
    if (n_caps > HUMAN_SLOTS) n_caps = HUMAN_SLOTS;
    if (minutes < 1) minutes = 1;

    FILE *f = fopen(path, "w");
    if (!f) { fprintf(stderr, "não consegui criar %s\n", path); return 1; }

    // Tudo sai da semente: o mapa e os sorteios do jogo e dos capitães.
    rng = seed * 2654435761u | 1;
    game_init(seed, seed * 2246822519u + 1, on_send);
    for (int i = 0; i < MAX_PLAYERS; i++) by_slot[i] = -1;
    for (int i = 0; i < n_caps; i++) join(i);
    n_npcs = game_add_npcs(n_npcs);

    printf("%d capitães e %d NPCs, %d min de jogo, semente %u -> %s\n", n_caps, n_npcs, minutes, seed, path);
    csv_header(f);
    clock_t start = clock();
    uint32_t end = (uint32_t)minutes * 60 * TICK_HZ;
    game_stats_t s;
    for (tick = 1; tick <= end; tick++) {
        for (int i = 0; i < n_caps; i++) think(&caps[i]);
        game_tick();
        if (tick % (60 * TICK_HZ) == 0) {
            game_stats(&s);
            csv_row(f, (int)(tick / (60 * TICK_HZ)), &s);
            if (tick % (10 * 60 * TICK_HZ) == 0)
                printf("[%3u min] capitães $%.2f | NPCs $%.2f, %u viagens, %u mortes, usando %d/%d/%d/%d | vendidos %u\n",
                       tick / (60 * TICK_HZ), s.money / 100.0, s.npc_money / 100.0, s.npc_trips, s.npc_deaths,
                       s.npc_using[0], s.npc_using[1], s.npc_using[2], s.npc_using[3], s.sold);
        }
    }
    fclose(f);

    double secs = (double)(clock() - start) / CLOCKS_PER_SEC;
    if (secs < 0.001) secs = 0.001;
    game_stats(&s);
    printf("\nFim: %d/%d capitães jogando, %u peixes vendidos por $%.2f, %u erros\n", s.online, n_caps, s.sold,
           s.earned / 100.0, errors);
    if (n_npcs) {
        printf("NPCs: $%.2f no total, %u viagens, %u mortes, %u reboques. Acham melhor:", s.npc_money / 100.0,
               s.npc_trips, s.npc_deaths, s.npc_tows);
        for (int i = 0; i < NPC_STRATEGIES; i++) printf(" %s %d", STRATEGY[i], s.npc_best[i]);
        printf("\n");
    }
    printf("%d min de jogo em %.1f s (%.0fx mais rápido que o real)\n", minutes, secs, minutes * 60.0 / secs);
    return errors ? 1 : 0;
}
