#include "game.h"
#include "world.h"
#include "chunks.h"
#include "fish.h"
#include "protocol.h"
#include "storage.h"
#include "items.h"
#include "password.h"
#include "boat.h"
#include "weapon.h"

#define MAX_SCHOOLS 288 // até 36 por barco (no Recife), com 8 barcos espalhados
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
// A sereia mora numa pedra no meio do Recife (ver "estruturas").
#define MERMAID_REACH 3   // até quantas células dá para falar com ela
#define MERMAID_SAFE_R 8  // zona segura em volta da pedra
#define SPIN_COOLDOWN (60 * TICK_HZ) // uma volta na roleta por minuto
#define STEAL_PCT 10
#define STEAL_MAX_CENTS 10000 // ela rouba 10% do dinheiro, no máximo $100
// Vilas de pescadores na costa das ilhas do Mangue e do Arquipélago: vendem e
// consertam, e cada uma paga mais por uma espécie (ver "estruturas").
#define VILLAGE_SAFE_R 10
#define VILLAGE_BONUS_PCT 25  // a mais pela espécie preferida

// Forte do armeiro: numa ilha perto do porto, mas fora da zona segura dele,
// para o jogador sair do porto para comprar a primeira arma.
#define ARMORY_MIN_DIST 40   // do porto
#define ARMORY_MAX_DIST 90
#define ARMORY_SPACING 30    // das vilas e das pedras da sereia
#define ARMORY_SAFE_R 8

// Mascate: em cada vila, ao lado do lojista. Vende itens épicos já
// encantados, uma unidade de cada; o catálogo de todas as vilas troca junto.
#define PEDDLER_ITEMS 2
#define CATALOG_TIME (30 * 60 * TICK_HZ)
#define PEDDLER_PRICE 4000000         // $40.000 por um épico
#define PEDDLER_ENCHANT_PRICE 1500000 // mais $15.000 por nível de encanto
// Encomenda do mascate: uns $300 em peixe (pelo preço cheio), de 5 a 50
// peixes. Paga o dobro do preço do mercado na hora da entrega, mais um baú.
#define QUEST_VALUE 30000
#define QUEST_MIN 5
#define QUEST_MAX 50
#define QUEST_PAY_PCT 200
#define QUEST_FRESH_PCT 80 // só pede espécie que está a 80% ou mais do preço cheio

// Tubarões: rondam o mar fundo e caçam barcos fracos. Não entram no raso
// nem nas zonas seguras, então a costa é um refúgio.
#define MAX_PREDATORS 68          // até 8 tubarões por barco (no Abismo) com 8 barcos, mais o navio fantasma
#define SHARK_RESPAWN (15 * TICK_HZ) // a cada 15 s, um tubarão novo em cada barco que tiver menos
#define SHARK_HP 40
#define SHARK_BITE 8              // dano de cada mordida
#define SHARK_BITE_EVERY 15       // ticks entre mordidas (1,5 s)
#define SHARK_SENSE 15            // de quantas células ele sente a presa
#define SHARK_WEAK_HP 120         // barco com menos vida que isso é presa
#define SHARK_SPEED_WANDER 3      // células por segundo, passeando
#define SHARK_SPEED_CHASE 7       // e caçando: o barco de fábrica faz 5
#define SHARK_BOUNTY_CENTS 1500   // $15 por tubarão afundado
// Eventos do mundo: de tempos em tempos acontece um, anunciado para todos.
#define EVENT_FIRST (2 * 60 * TICK_HZ) // o primeiro vem entre 2 e 4 min
#define EVENT_GAP (4 * 60 * TICK_HZ)   // depois, um a cada 4 a 8 min
#define EVENT_MIN_DIST 40              // longe do porto
#define STORM_R 25
#define STORM_TIME (120 * TICK_HZ)
#define STORM_DRIFT_EVERY (2 * TICK_HZ) // a tempestade anda 1 célula a cada 2 s
#define STORM_PUSH_EVERY TICK_HZ        // o vento empurra o barco 1 célula por segundo
#define STORM_HURT_EVERY (2 * TICK_HZ)  // as ondas tiram 1 de vida a cada 2 s
#define STORM_CATCH_PCT 200             // mas o peixe sobe: pesca em dobro
#define SQUID_R 20
#define SQUID_TIME (90 * TICK_HZ)
#define SQUID_SCHOOLS 6
// Navio fantasma: aparece como evento, atira em qualquer barco perto e só
// afunda com muito dano. O prêmio é dividido pelo dano que cada um deu.
#define GHOST_TIME (150 * TICK_HZ)
#define GHOST_R 30                 // área anunciada em volta do navio
#define GHOST_HP 300
#define GHOST_SENSE 20             // de quantas células ele vê um barco
#define GHOST_KEEP 6               // chega até essa distância e fica atirando
#define GHOST_SPEED 4              // células por segundo
#define GHOST_RANGE 12
#define GHOST_RELOAD (2 * TICK_HZ)
#define GHOST_DAMAGE 10
#define GHOST_SHOT_SPEED 2
#define GHOST_BOUNTY_CENTS 30000   // $300 divididos entre quem acertou

// Clima (v0.9): vale para o mapa todo e se alterna com o tempo limpo.
// Chuva: o peixe sobe e a pesca rende mais. Neblina: o servidor só conta
// onde estão os barcos e os bichos que estão a menos de FOG_VIEW células.
#define WEATHER_FIRST (3 * 60 * TICK_HZ) // o primeiro vem entre 3 e 6 min
#define WEATHER_GAP (3 * 60 * TICK_HZ)   // tempo limpo entre um e outro: 3 a 6 min
#define WEATHER_TIME (2 * 60 * TICK_HZ)  // chuva ou neblina duram 2 a 4 min
#define RAIN_CATCH_PCT 125               // chuva: pesca 25% maior
#define FOG_VIEW 16                      // neblina: só se vê até 16 células

// Poderes das armas únicas (weapon_t.fx).
#define LEECH_PCT 25          // Dente: % do dano que volta como conserto
#define GRAB_TICKS (2 * TICK_HZ) // Tentáculo: tempo que o alvo fica preso

// Bosses: um por vez, numa agenda própria (pode coincidir com tempestade e afins).
// Para testar, compile com -DBOSS_FIRST=100 (o primeiro vem em 10 s).
#ifndef BOSS_FIRST
#define BOSS_FIRST (10 * 60 * TICK_HZ) // o primeiro vem entre 10 e 20 min
#endif
#define BOSS_GAP (20 * 60 * TICK_HZ)   // depois, um a cada 20 a 40 min
#define BOSS_TIME (5 * 60 * TICK_HZ)   // tempo para afundar o boss antes que ele vá embora
#define BOSS_MIN_DIST 60               // longe do porto
#define BOSS_SENSE 25                  // de quantas células ele vê um barco
#define BOSS_HELPER_PCT 10             // quem deu 10% do dano ganha um baú

#define MEGA_SPEED 5          // células por segundo, nadando
#define MEGA_CHARGE_SPEED 12  // na investida: nenhum barco foge
#define MEGA_CHARGE_FROM 12   // começa a investida a essa distância
#define MEGA_CHARGE_TIME (2 * TICK_HZ)
#define MEGA_REST (4 * TICK_HZ) // depois de morder (ou errar), descansa
#define MEGA_BITE 25

#define KRAKEN_REACH 12              // alcance dos tentáculos: canhão (18) e mosquete (14) batem de fora
#define KRAKEN_ARMS 3                // quantos barcos ele agarra de uma vez
#define KRAKEN_EVERY (3 * TICK_HZ)   // um ataque a cada 3 s
#define KRAKEN_DAMAGE 12
#define KRAKEN_PULL 2                // células que o tentáculo puxa o barco para perto

#define CTHULHU_SPEED 2                // células por segundo: lento, mas vai em qualquer água
#define CTHULHU_RING_EVERY (5 * TICK_HZ) // a cada 5 s, um anel de 8 tiros
#define CTHULHU_RING_DAMAGE 12
#define CTHULHU_RING_RANGE 16
#define CTHULHU_RING_SPEED 2
#define CTHULHU_MADNESS_R 15           // perto dele, o leme fica invertido

// Mecânico: preço pela raridade mínima do item que sai. Nunca sai comum:
// dois comuns já dão um raro.
static const uint32_t COMBINE_PRICE[RARITY_COUNT] = {0, 8000, 25000, 60000};

// Mercado: vender 200 de uma espécie derruba o preço pela metade,
// nunca abaixo de 30%, e a pressão cai 1% por segundo.
#define MARKET_SATURATION 200000 // em milésimos de peixe
#define MARKET_MIN_PCT 30

// Ecossistema: o mar é dividido em regiões de 32x32 células, e cada uma tem
// um estoque de peixe. Pescar tira do estoque, e com menos peixe a chance de
// pescar ali cai junto. O estoque volta sozinho com o tempo.
#define REGION_SIZE 32
#define STOCK_MAX 250                   // cabe num uint8_t
#define STOCK_REGEN_EVERY (5 * TICK_HZ) // a cada 5 s as regiões crescem
#define STOCK_GROWTH 2500               // maior = cresce mais devagar
#define SCHOOL_MIN_STOCK (STOCK_MAX / 5) // com menos de 20% o cardume vai embora

typedef enum { SLOT_FREE = 0, SLOT_CONNECTED, SLOT_PLAYING } slot_state_t;

typedef struct {
    slot_state_t state;
    int profile; // índice em g.profiles
    int32_t x, y;
    int8_t dx, dy;       // para onde o jogador quer ir (teclado)
    uint8_t heading;     // para onde a proa aponta (0 a 7, ver DIR_X)
    uint8_t turn_timer;  // ticks desde a última virada de 45°
    uint8_t color;
    uint8_t speed;       // velocidade máxima, células por segundo
    uint8_t vel;         // velocidade agora: sobe até `speed` e cai devagar
    uint8_t move_budget; // em décimos de célula
    uint8_t blocked;
    uint8_t reload;
    uint8_t grabbed;    // ticks que ainda fica preso pelo Tentáculo do Kraken
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
    uint8_t owner;       // slot de quem atirou, ou KILLER_PREDATOR + tipo do bicho
    uint8_t speed;       // células por tick
    uint8_t left;        // células que ainda pode andar
    int32_t x, y;        // em 1/SUB de célula
    int32_t ux, uy;      // um passo de 1 célula na direção do tiro, em 1/SUB
    uint16_t damage;
    uint8_t fx;          // poder da arma que atirou (weapon_fx_t)
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
    int32_t x, y;
    int16_t r;
} safe_zone_t;

typedef struct {
    uint8_t alive, kind;
    int32_t x, y;
    int16_t hp;
    int8_t target;       // slot do barco que ele caça (-1 = nenhum)
    uint8_t heading;     // para onde nada quando só está passeando (0 a 7)
    uint8_t move_budget; // em décimos de célula, igual ao barco
    uint8_t bite;        // ticks até poder morder de novo
    uint8_t grabbed;     // ticks que ainda fica preso pelo Tentáculo
} predator_t;

// O evento do mundo que está acontecendo agora (só um por vez).
typedef struct {
    uint8_t kind;  // WORLD_* (WORLD_NONE = nada acontecendo)
    int32_t x, y;
    int16_t r;
    int8_t dx, dy; // para onde sopra o vento da tempestade
    uint32_t ends; // tick em que acaba
} world_event_t;

// O clima agora e quando ele muda.
typedef struct {
    uint8_t kind;  // WEATHER_*
    uint32_t ends; // tick em que muda (para limpo ou para o próximo)
} weather_t;

// Cada boss: vida (base + um tanto por jogador online), tamanho, prêmio e arma única.
typedef struct {
    uint16_t hp_base, hp_per_player;
    uint8_t r; // raio do corpo: o tiro acerta a até r células do centro
    uint32_t bounty_cents;
    uint8_t weapon;
} boss_info_t;

static const boss_info_t BOSSES[BOSS_COUNT] = {
    [BOSS_MEGALODON] = {600, 300, 2, 150000, ITEM_DENTE},
    [BOSS_KRAKEN] = {900, 450, 3, 250000, ITEM_TENTACULO},
    [BOSS_CTHULHU] = {1500, 600, 4, 500000, ITEM_CAJADO},
};

typedef struct {
    uint8_t kind;        // BOSS_* (BOSS_NONE = nenhum no mar)
    int32_t x, y;
    int16_t hp, hp_max;
    int8_t target;       // slot do barco que ele persegue (-1 = nenhum)
    uint8_t heading;     // para onde nada sem alvo (0 a 7)
    uint8_t move_budget; // em décimos de célula
    uint8_t grabbed;     // preso pelo Tentáculo (sim, até o boss)
    uint8_t attack;      // ticks de ataque: investida do Megalodon, tentáculos do Kraken, anel do Cthulhu
    uint8_t rest;        // ticks até poder atacar de novo
    uint32_t ends;       // tick em que vai embora
    uint16_t dmg[MAX_PLAYERS]; // dano que cada slot deu nele
} boss_t;

// Zonas seguras fixas. As das vilas e da sereia vêm das estruturas (in_safe_zone).
enum { ZONE_PORT = 0, ZONE_ARMORY, SAFE_ZONE_COUNT };

// Estruturas: vilas e pedras da sereia não são mais fixas. Cada região do
// mapa pode ter uma, sorteada pela semente (sempre a mesma no mesmo mapa). O
// servidor só descobre o que tem numa região quando algum barco chega perto
// (as 3x3 regiões em volta dele), porque achar o cais precisa olhar os tiles.
#define STRUCT_SLOTS 2048  // hash por região; com metade cheia (1024 regiões vistas), recomeça do zero
#define MAX_VILLAGES 200   // vilas com catálogo e encomenda; SHOP_VILLAGE + índice cabe num u8
#define STRUCT_SEARCH 48   // o cais ou a pedra fica a até 48 células do centro da região
#define VILLAGE_CHANCE 3   // 1 em 3 regiões de Mangue ou Arquipélago tem vila
#define MERMAID_CHANCE 2   // 1 em 2 regiões de Recife tem pedra da sereia

enum { STRUCT_NONE = 0, STRUCT_VILLAGE, STRUCT_MERMAID };

typedef struct {
    int16_t rx, ry;
    uint8_t used, kind; // kind: STRUCT_*
    int32_t x, y;       // cais da vila ou pedra da sereia
    int16_t village;    // índice em g.villages (só vila)
} structure_t;

// Cais de uma vila: a célula de água colada na ilha onde o barco encosta.
typedef struct {
    int32_t x, y;
    uint8_t fav; // espécie que ela paga mais caro
    // Catálogo do mascate. Vendido: base = ITEM_NONE até a próxima troca.
    item_t offer[PEDDLER_ITEMS];
    uint8_t offer_enchant[PEDDLER_ITEMS];
    // Encomenda: quantos peixes de que espécie. Cada perfil entrega uma vez
    // por catálogo (um bit por perfil, zerado na troca).
    uint8_t quest_species;
    uint8_t quest_count;
    uint8_t quest_done[MAX_PROFILES / 8];
} village_t;

// Regiões de teste: o cliente gera estas assim que entra e confere com o
// checksum do servidor. Ficam perto do porto, no mar distante e no abismo
// (anel 40), para pegar diferença em qualquer parte da conta dos biomas.
#define PROBE_COUNT 8
static const int8_t PROBES[PROBE_COUNT][2] = {{0, 0}, {1, 0}, {0, -1}, {-2, 2}, {3, -3}, {-7, 5}, {12, 9}, {-40, -25}};

// Estoque esparso: a tabela só guarda as regiões de 32x32 que alguém pescou e
// que ainda não encheram de novo. Região fora da tabela está cheia.
#define STOCK_SLOTS 8192 // potência de 2; fica no máximo meio cheia (4096 regiões)
typedef struct {
    int16_t rx, ry; // região (célula / REGION_SIZE): até 2048, cabe em int16
    uint8_t used, stock;
} stock_cell_t;

static struct {
    uint32_t seed, tick, rng;
    uint32_t probe_sum[PROBE_COUNT]; // checksum das regiões de teste, para o cliente conferir
    game_send_fn send;
    player_t players[MAX_PLAYERS];
    school_t schools[MAX_SCHOOLS];
    shot_t shots[MAX_SHOTS];
    drop_t drops[MAX_DROPS];
    profile_t profiles[MAX_PROFILES];
    uint32_t pressure[FISH_COUNT];
    safe_zone_t safe_zones[SAFE_ZONE_COUNT];
    stock_cell_t stock[STOCK_SLOTS]; // só as regiões pescadas (ver stock_find)
    int stock_used;
    structure_t structures[STRUCT_SLOTS]; // o que tem em cada região já vista (ver struct_resolve)
    int struct_used;
    village_t villages[MAX_VILLAGES]; // as vilas já descobertas, na ordem em que apareceram
    int village_count;
    uint32_t catalog_ends; // tick em que os mascates trocam o catálogo
    predator_t predators[MAX_PREDATORS];
    uint32_t next_shark; // tick em que pode nascer outro tubarão
    world_event_t event;
    uint32_t next_event; // tick em que começa o próximo evento
    int ghost;           // índice do navio fantasma em `predators` (-1 = não tem)
    uint16_t ghost_dmg[MAX_PLAYERS]; // dano que cada slot deu nele
    boss_t boss;
    uint32_t next_boss; // tick em que aparece o próximo boss
    weather_t weather;
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

static tile_t tile(int32_t x, int32_t y) { return chunks_tile(x, y); }

// Procura a região (rx, ry) na tabela de estoque (hash com sondagem linear).
// Se não achar e `create` for 1, entra com o estoque cheio. Devolve NULL se
// não achou (e não criou) ou se a tabela já está meio cheia: aí a região fica
// cheia para sempre até a próxima limpeza, o que só acontece com milhares de
// regiões pescadas ao mesmo tempo.
static stock_cell_t *stock_slot(int rx, int ry, int create) {
    for (uint32_t i = world_hash(rx, ry, 0x570C) & (STOCK_SLOTS - 1);; i = (i + 1) & (STOCK_SLOTS - 1)) {
        stock_cell_t *c = &g.stock[i];
        if (!c->used) {
            if (!create || g.stock_used >= STOCK_SLOTS / 2) return NULL;
            *c = (stock_cell_t){(int16_t)rx, (int16_t)ry, 1, STOCK_MAX};
            g.stock_used++;
            return c;
        }
        if (c->rx == rx && c->ry == ry) return c;
    }
}

// A região da célula (x, y) na tabela.
static stock_cell_t *stock_find(int x, int y, int create) {
    x = x < 0 ? 0 : x >= MAP_W ? MAP_W - 1 : x;
    y = y < 0 ? 0 : y >= MAP_H ? MAP_H - 1 : y;
    return stock_slot(x / REGION_SIZE, y / REGION_SIZE, create);
}

// Estoque da região onde fica a célula (x, y): fora da tabela = cheia.
static int stock_get(int x, int y) {
    const stock_cell_t *c = stock_find(x, y, 0);
    return c ? c->stock : STOCK_MAX;
}

// Chance de pescar (por mil) ajustada pelo estoque: região cheia = chance
// inteira, região pela metade = metade da chance, região vazia = nada.
static uint32_t with_stock(uint32_t permille, int x, int y) {
    return permille * (uint32_t)stock_get(x, y) / STOCK_MAX;
}

// Bioma da região onde fica a célula (x, y).
static biome_t biome_at(int x, int y) {
    return world_region_biome(world_region_of(x), world_region_of(y), g.seed);
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

// Distância² em 64 bits: no mundo de 65536 células, dx² + dy² passa de 32 bits.
static int64_t dist2(int dx, int dy) { return (int64_t)dx * dx + (int64_t)dy * dy; }

// Na seção "estruturas", mais abaixo.
static const structure_t *struct_get(int rx, int ry);
static void struct_resolve_near(int x, int y);

// Estrutura do tipo `kind` a até `reach` células (num quadrado) de (x, y).
// Olha só as 3x3 regiões em volta, e só as já descobertas: perto dos barcos
// elas sempre estão.
static const structure_t *struct_near(int x, int y, int kind, int reach) {
    int rx0 = world_region_of(x), ry0 = world_region_of(y);
    for (int ry = ry0 - 1; ry <= ry0 + 1; ry++)
        for (int rx = rx0 - 1; rx <= rx0 + 1; rx++) {
            const structure_t *s = struct_get(rx, ry);
            if (s && s->kind == kind && s->x >= x - reach && s->x <= x + reach && s->y >= y - reach && s->y <= y + reach)
                return s;
        }
    return 0;
}

static int in_safe_zone(int x, int y) {
    for (int i = 0; i < SAFE_ZONE_COUNT; i++) {
        const safe_zone_t *z = &g.safe_zones[i];
        if (z->r <= 0) continue; // forte que não coube no mapa
        int dx = x - z->x, dy = y - z->y;
        if (dist2(dx, dy) <= z->r * z->r) return 1;
    }
    const structure_t *s = struct_near(x, y, STRUCT_VILLAGE, VILLAGE_SAFE_R);
    if (s && dist2(x - s->x, y - s->y) <= VILLAGE_SAFE_R * VILLAGE_SAFE_R) return 1;
    s = struct_near(x, y, STRUCT_MERMAID, MERMAID_SAFE_R);
    return s && dist2(x - s->x, y - s->y) <= MERMAID_SAFE_R * MERMAID_SAFE_R;
}

// Índice da vila em cujo cais o barco está, ou -1.
static int at_village(const player_t *p) {
    for (int i = 0; i < g.village_count; i++) {
        const village_t *v = &g.villages[i];
        if (p->x >= v->x - 1 && p->x <= v->x + 1 && p->y >= v->y - 1 && p->y <= v->y + 1) return i;
    }
    return -1;
}

// Lojista e reparo: no porto ou em qualquer vila. O resto (estaleiro,
// mercenário, mecânico) continua só no porto.
static int at_shop(const player_t *p) { return at_dock(p) || at_village(p) >= 0; }

// No cais do forte do armeiro (r = 0: o forte não coube no mapa).
static int at_armory(const player_t *p) {
    const safe_zone_t *z = &g.safe_zones[ZONE_ARMORY];
    return z->r > 0 && p->x >= z->x - 1 && p->x <= z->x + 1 && p->y >= z->y - 1 && p->y <= z->y + 1;
}

static int near_mermaid(const player_t *p) { return struct_near(p->x, p->y, STRUCT_MERMAID, MERMAID_REACH) != 0; }

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

static int iabs(int v) { return v < 0 ? -v : v; }

static int far_from(int x, int y, int ox, int oy, int dist) {
    int dx = x - ox, dy = y - oy;
    return dist2(dx, dy) >= (int64_t)dist * dist;
}

// O mundo é enorme, mas só tem vida perto dos barcos. Cada barco tem uma
// "bolha" em volta (SPAWN_RANGE para cada lado), e o servidor mantém nela
// cardumes e tubarões conforme o bioma (BIOME_SCHOOL_PCT e BIOME_SHARKS).
// O que já está na bolha conta para todos os barcos que estão ali: barcos
// juntos dividem os bichos em vez de encher o mar. Assim só a parte perto
// dos barcos vira chunk.
#define SPAWN_RANGE 256
#define SCHOOLS_PER_BOAT 30 // no bioma "normal" (100%); a tabela abaixo muda por bioma
// Tubarão e cardume que ficam mais longe que isso de todos os barcos somem,
// e a vaga volta a nascer perto de alguém.
#define DESPAWN_RANGE 512

// Uma célula a até SPAWN_RANGE de (cx, cy), fora da tela de quem está lá
// (VIEW_RADIUS), para nada aparecer do nada na frente de ninguém.
static void cell_near(int cx, int cy, int *x, int *y) {
    int dx, dy;
    do { // sorteia de novo enquanto cair dentro da tela (cai lá 1 vez em 7, mais ou menos)
        dx = (int)rnd_below(2 * SPAWN_RANGE + 1) - SPAWN_RANGE;
        dy = (int)rnd_below(2 * SPAWN_RANGE + 1) - SPAWN_RANGE;
    } while (iabs(dx) <= VIEW_RADIUS && iabs(dy) <= VIEW_RADIUS);
    *x = cx + dx, *y = cy + dy;
}

// Perto de um barco sorteado entre os que estão jogando (sem ninguém, perto
// do porto). Para eventos e bosses, que são um só no mar todo.
static void random_cell(int *x, int *y) {
    int cx = PORT_X, cy = PORT_Y, online[MAX_PLAYERS], n = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) online[n++] = i;
    if (n) {
        const player_t *p = &g.players[online[rnd_below((uint32_t)n)]];
        cx = p->x, cy = p->y;
    }
    cell_near(cx, cy, x, y);
}

// Quanto cada bioma tem de vida, olhando o bioma onde o barco está:
// cardumes em % de SCHOOLS_PER_BOAT, e tubarões por bolha.
static const uint8_t BIOME_SCHOOL_PCT[BIOME_COUNT] = {
    [BIOME_BAY] = 90, [BIOME_MANGROVE] = 110, [BIOME_ISLANDS] = 100,
    [BIOME_OPEN] = 80, [BIOME_REEF] = 120, [BIOME_ABYSS] = 50,
};
static const uint8_t BIOME_SHARKS[BIOME_COUNT] = {
    [BIOME_BAY] = 2, [BIOME_MANGROVE] = 3, [BIOME_ISLANDS] = 4,
    [BIOME_OPEN] = 6, [BIOME_REEF] = 5, [BIOME_ABYSS] = 8,
};

// (x, y) está na bolha do barco?
static int in_bubble(const player_t *p, int x, int y) {
    return iabs(x - p->x) <= SPAWN_RANGE && iabs(y - p->y) <= SPAWN_RANGE;
}

// Longe de todos os barcos? Sem ninguém jogando, nada está longe (nada some).
static int far_from_boats(int x, int y) {
    int any = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        const player_t *p = &g.players[i];
        if (p->state != SLOT_PLAYING) continue;
        if (!far_from(x, y, p->x, p->y, DESPAWN_RANGE)) return 0;
        any = 1;
    }
    return any;
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

// Pontos do minimapa de um barco. Cardumes só até MARK_RANGE dele (o mapa
// grande mostra 2048 células), e no máximo MARK_SCHOOLS_MAX, para caber no
// buffer junto com bags, coroas e lugares.
#define MARK_RANGE 1024
#define MARK_SCHOOLS_MAX 120

static int near(int x, int y, int ox, int oy, int extra); // na tela do barco (mais abaixo)

static void send_marks_to(int slot) {
    const player_t *p = &g.players[slot];
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_MARKS);
    size_t count_at = w.len;
    put_u8(&w, 0);
    uint8_t n = 0, schools = 0;
    for (int i = 0; i < MAX_SCHOOLS && schools < MARK_SCHOOLS_MAX; i++) {
        const school_t *s = &g.schools[i];
        if (!s->alive || iabs(s->x - p->x) > MARK_RANGE || iabs(s->y - p->y) > MARK_RANGE) continue;
        schools++;
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
    // Forte, vilas e sereia só aparecem quando estão na tela do barco: o
    // resto do mapa o jogador descobre navegando.
    const safe_zone_t *armory = &g.safe_zones[ZONE_ARMORY];
    if (armory->r > 0 && near(armory->x, armory->y, p->x, p->y, 0)) {
        put_u16(&w, (uint16_t)armory->x);
        put_u16(&w, (uint16_t)armory->y);
        put_u8(&w, MARK_ARMORY);
        put_u8(&w, 0);
        n++;
    }
    int rx0 = world_region_of(p->x), ry0 = world_region_of(p->y);
    for (int ry = ry0 - 1; ry <= ry0 + 1; ry++)
        for (int rx = rx0 - 1; rx <= rx0 + 1; rx++) {
            const structure_t *st = struct_get(rx, ry);
            if (!st || st->kind == STRUCT_NONE || !near(st->x, st->y, p->x, p->y, 0)) continue;
            put_u16(&w, (uint16_t)st->x);
            put_u16(&w, (uint16_t)st->y);
            put_u8(&w, st->kind == STRUCT_VILLAGE ? MARK_VILLAGE : MARK_MERMAID);
            put_u8(&w, st->kind == STRUCT_VILLAGE ? g.villages[st->village].fav : 0);
            n++;
        }
    out[count_at] = n;
    send_w(slot, &w);
}

// slot < 0: para todo mundo (cada um recebe os pontos perto dele).
static void send_marks(int slot) {
    if (slot >= 0) { send_marks_to(slot); return; }
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) send_marks_to(i);
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
// Um item "aberto" na rede: 17 bytes (o mesmo formato no S_INVENTORY e no S_CATALOG).
static void put_item(writer_t *w, const item_info_t *info, uint8_t equipped, uint8_t enchant) {
    put_u8(w, info->base);
    put_u8(w, info->rarity);
    put_u8(w, equipped);
    put_u8(w, enchant);
    for (int a = 0; a < MAX_AFFIXES; a++) put_u8(w, info->affix[a]);
    for (int s = 0; s < STAT_COUNT; s++) put_i16(w, info->stats[s]);
}

static void send_inventory(int slot) {
    const player_t *p = &g.players[slot];
    const profile_t *pr = prof(p);
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_INVENTORY);
    put_u8(&w, BAG_SIZE);
    for (int i = 0; i < BAG_SIZE; i++) {
        item_info_t info = bag_info(pr, i);
        put_item(&w, &info, equipped_slot(pr, i), pr->enchant[i]);
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

// Perto o bastante para aparecer na tela e, na neblina, perto o bastante para ver.
static int visible(int x, int y, const player_t *p) {
    if (!near(x, y, p->x, p->y, 0)) return 0;
    if (g.weather.kind != WEATHER_FOG) return 1;
    int dx = x - p->x, dy = y - p->y;
    return dist2(dx, dy) <= FOG_VIEW * FOG_VIEW;
}

static int is_mad(const player_t *p); // na seção dos bosses

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
                   (in_safe_zone(p->x, p->y) ? STATE_SAFE : 0) | (near_mermaid(p) ? STATE_AT_MERMAID : 0) |
                   (at_village(p) >= 0 ? STATE_AT_VILLAGE : 0) | (p->grabbed ? STATE_GRABBED : 0) |
                   (is_mad(p) ? STATE_MAD : 0) | (at_armory(p) ? STATE_AT_ARMORY : 0));
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
        if (i == slot || o->state != SLOT_PLAYING || !visible(o->x, o->y, p)) continue;
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

    // Desde a v7: estoque de peixe da região, bichos por perto e o evento do mundo.
    put_u8(&w, (uint8_t)(stock_get(p->x, p->y) * 100 / STOCK_MAX));
    count_at = w.len;
    put_u8(&w, 0);
    n = 0;
    for (int i = 0; i < MAX_PREDATORS; i++) {
        const predator_t *pd = &g.predators[i];
        if (!pd->alive || !visible(pd->x, pd->y, p)) continue;
        int max = pd->kind == PRED_GHOST ? GHOST_HP : SHARK_HP;
        put_u8(&w, pd->kind);
        put_u16(&w, (uint16_t)pd->x);
        put_u16(&w, (uint16_t)pd->y);
        put_u8(&w, (uint8_t)(pd->hp * 100 / max));
        n++;
    }
    out[count_at] = n;
    const world_event_t *ev = &g.event;
    put_u8(&w, ev->kind);
    put_u16(&w, (uint16_t)ev->x);
    put_u16(&w, (uint16_t)ev->y);
    put_u8(&w, (uint8_t)ev->r);
    put_u16(&w, (uint16_t)(ev->kind && ev->ends > g.tick ? (ev->ends - g.tick + TICK_HZ - 1) / TICK_HZ : 0));
    put_u8(&w, (uint8_t)ev->dx);
    put_u8(&w, (uint8_t)ev->dy);

    // Desde a v8: o boss. Vai para todo mundo, perto ou longe, para a barra de
    // vida aparecer na tela de quem está indo ajudar.
    const boss_t *b = &g.boss;
    put_u8(&w, b->kind);
    put_u16(&w, (uint16_t)b->x);
    put_u16(&w, (uint16_t)b->y);
    put_u8(&w, b->kind ? BOSSES[b->kind].r : 0);
    put_u16(&w, (uint16_t)(b->hp > 0 ? b->hp : 0));
    put_u16(&w, (uint16_t)b->hp_max);
    put_u16(&w, (uint16_t)(b->kind && b->ends > g.tick ? (b->ends - g.tick + TICK_HZ - 1) / TICK_HZ : 0));
    put_u8(&w, b->attack);

    // Desde a v9: o clima, igual no mapa todo.
    put_u8(&w, g.weather.kind);
    put_u16(&w, (uint16_t)(g.weather.ends > g.tick ? (g.weather.ends - g.tick + TICK_HZ - 1) / TICK_HZ : 0));
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

// Baú: um item aleatório de qualquer tipo (menos as armas únicas dos bosses).
static void give_random_item(int slot) {
    player_t *p = &g.players[slot];
    item_t item = item_new((uint8_t)(1 + rnd_below(ITEM_DROP_COUNT - 1)), rnd());
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
    // Não dá para pescar mais peixe do que a região tem.
    stock_cell_t *c = stock_find(p->x, p->y, 1);
    int stock = c ? c->stock : STOCK_MAX;
    int got = n < room ? n : room;
    if (got > stock) got = stock;
    if (got <= 0) return;
    if (c) c->stock = (uint8_t)(stock - got);
    prof(p)->hold[species] += got;
    send_event(slot, EV_CATCH, species, (uint16_t)got, 0);
    int luck = p->stats[STAT_LUCK] > 0 ? p->stats[STAT_LUCK] : 0;
    if (chance_permille(CHEST_PER_MILLE + (uint32_t)luck)) give_random_item(slot);
}

static void sell(int slot, uint8_t species) {
    player_t *p = &g.players[slot];
    if (!at_shop(p)) return;
    profile_t *pr = prof(p);
    int v = at_village(p);
    uint8_t fav = v >= 0 ? g.villages[v].fav : FISH_NONE;
    uint32_t total = 0, count = 0;
    for (int i = 0; i < FISH_COUNT; i++) {
        if (species != FISH_NONE && species != i) continue;
        uint32_t n = pr->hold[i];
        if (!n) continue;
        uint32_t price = price_cents(i);
        if (i == fav) price = price * (100 + VILLAGE_BONUS_PCT) / 100;
        total += price * n;
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
    if (!at_shop(p)) return;
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
    // Arma única não se combina: já é lendária e não existe outra igual.
    if (ia.base == ITEM_NONE || ia.base != ib.base || item_is_unique(ia.base)) {
        send_event(slot, EV_CANT_COMBINE, FISH_NONE, 0, 0);
        return;
    }

    uint8_t ra = item_info(ia).rarity, rb = item_info(ib).rarity;
    uint8_t want = ra > rb ? ra : rb;
    if (ra == rb && want < RARITY_LEGENDARY) want++;
    uint32_t price = COMBINE_PRICE[want];
    if (pr->money < price) { send_event(slot, EV_NO_MONEY, FISH_NONE, 0, price); return; }

    // Sorteia até sair no mínimo a raridade pedida (lendário é 1 em 100).
    item_t made = item_in_rarity(ia.base, want, RARITY_LEGENDARY, rnd());

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

// Armeiro: vende arma nova com a raridade garantida; os números e afixos
// saem sorteados, como num baú. Só cobra se couber na bolsa.
static void buy_armory(int slot, uint8_t offer) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (!at_armory(p) || offer >= ARMORY_OFFERS) return;
    const armory_offer_t *o = &ARMORY[offer];
    if (pr->money < o->price) { send_event(slot, EV_NO_MONEY, FISH_NONE, 0, o->price); return; }
    item_t item = item_in_rarity(o->base, o->rarity, o->rarity, rnd());
    if (bag_add(pr, item) < 0) { send_event(slot, EV_BAG_FULL, FISH_NONE, 0, 0); return; }
    pr->money -= o->price;
    storage_save(p->profile, pr); // dinheiro gasto: salva já
    send_event(slot, EV_BOUGHT, item.base, o->rarity, o->price);
    send_inventory(slot);
    send_rank(-1);
}

static uint32_t peddler_price(uint8_t enchant) { return PEDDLER_PRICE + PEDDLER_ENCHANT_PRICE * enchant; }

// Catálogo do mascate da vila onde o barco está (nada se não está numa vila).
static void send_catalog(int slot) {
    const player_t *p = &g.players[slot];
    int vi = at_village(p);
    if (vi < 0) return;
    const village_t *v = &g.villages[vi];
    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_CATALOG);
    put_u8(&w, (uint8_t)vi);
    put_u16(&w, (uint16_t)((g.catalog_ends - g.tick) / TICK_HZ));
    put_u8(&w, PEDDLER_ITEMS);
    for (int k = 0; k < PEDDLER_ITEMS; k++) {
        item_info_t info = item_info_enchanted(v->offer[k], v->offer_enchant[k]); // vendido: tipo 0
        put_item(&w, &info, 0, v->offer[k].base ? v->offer_enchant[k] : 0);
        put_u32(&w, peddler_price(v->offer_enchant[k]));
    }
    put_u8(&w, v->quest_species);
    put_u8(&w, v->quest_count);
    put_u8(&w, (v->quest_done[p->profile / 8] >> (p->profile % 8)) & 1);
    send_w(slot, &w);
}

// Para todo mundo que está numa vila: depois de uma compra ou de uma troca.
static void send_catalogs(void) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) send_catalog(i);
}

// Mascate: quem chegar primeiro leva. O item vem com o encanto já posto.
static void buy_peddler(int slot, int village, uint8_t offer) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    if (village >= g.village_count || at_village(p) != village || offer >= PEDDLER_ITEMS) return;
    village_t *v = &g.villages[village];
    if (v->offer[offer].base == ITEM_NONE) { send_event(slot, EV_SOLD_OUT, FISH_NONE, 0, 0); return; }
    uint32_t price = peddler_price(v->offer_enchant[offer]);
    if (pr->money < price) { send_event(slot, EV_NO_MONEY, FISH_NONE, 0, price); return; }
    int index = bag_add(pr, v->offer[offer]);
    if (index < 0) { send_event(slot, EV_BAG_FULL, FISH_NONE, 0, 0); return; }
    pr->enchant[index] = v->offer_enchant[offer];
    pr->money -= price;
    storage_save(p->profile, pr);
    send_event(slot, EV_BOUGHT, v->offer[offer].base, RARITY_EPIC, price);
    v->offer[offer] = (item_t){0}; // esgotado até a próxima troca
    send_inventory(slot);
    send_catalogs(); // quem mais está na vila vê o "vendido" na hora
    send_rank(-1);
}

// Encomenda: entrega os peixes pedidos. O preço é o do mercado agora, então
// se a turma vendeu muito dessa espécie, a encomenda paga menos. A entrega
// também pesa no mercado, como uma venda.
static void deliver(int slot) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    int vi = at_village(p);
    if (vi < 0) return;
    village_t *v = &g.villages[vi];
    uint8_t sp = v->quest_species, bit = (uint8_t)(1 << (p->profile % 8));
    if (v->quest_done[p->profile / 8] & bit) { send_event(slot, EV_QUEST_WAIT, sp, 0, 0); return; }
    if (pr->hold[sp] < v->quest_count) { send_event(slot, EV_QUEST_MISSING, sp, v->quest_count, pr->hold[sp]); return; }
    uint32_t pay = price_cents(sp) * v->quest_count * QUEST_PAY_PCT / 100;
    pr->hold[sp] -= v->quest_count;
    g.pressure[sp] += (uint32_t)v->quest_count * 1000;
    pr->money += pay;
    v->quest_done[p->profile / 8] |= bit;
    storage_save(p->profile, pr);
    send_event(slot, EV_QUEST_DONE, sp, v->quest_count, pay);
    give_random_item(slot); // o baú (se a bolsa estiver cheia, avisa)
    send_catalog(slot);
    send_prices(-1);
    send_rank(-1);
}

static void buy(int slot, uint8_t shop, uint8_t offer) {
    if (shop == SHOP_ARMORY) buy_armory(slot, offer);
    else if (shop >= SHOP_VILLAGE) buy_peddler(slot, shop - SHOP_VILLAGE, offer);
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
    int hit = p->vel >= CRASH_MIN_SPEED && !p->blocked && !at_shop(p);
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

// Um tiro de (x, y) na direção de (tx, ty). Serve para os barcos e para o
// navio fantasma. Devolve 0 se não saiu (mira em cima de si ou mar lotado de tiros).
static int spawn_shot(uint8_t owner, int x, int y, int tx, int ty, uint8_t speed, uint8_t range, uint16_t damage,
                      uint8_t fx) {
    int dx = tx - x, dy = ty - y;
    uint32_t len = isqrt((uint32_t)(dx * dx + dy * dy));
    if (len == 0) return 0;
    for (int i = 0; i < MAX_SHOTS; i++) {
        shot_t *sh = &g.shots[i];
        if (sh->alive) continue;
        *sh = (shot_t){
            .alive = 1,
            .owner = owner,
            .speed = speed,
            .left = range,
            .x = x * SUB,
            .y = y * SUB,
            .ux = dx * SUB / (int32_t)len,
            .uy = dy * SUB / (int32_t)len,
            .damage = damage,
            .fx = fx,
        };
        return 1;
    }
    return 0;
}

static void fire(int slot, int tx, int ty) {
    player_t *p = &g.players[slot];
    const weapon_t *wp = weapon(p);
    if (!wp || p->reload) return;
    if (in_safe_zone(p->x, p->y)) { send_event(slot, EV_SAFE_ZONE, FISH_NONE, 0, 0); return; }
    uint16_t damage = (uint16_t)(p->stats[STAT_DAMAGE] > 1 ? p->stats[STAT_DAMAGE] : 1);
    if (!spawn_shot((uint8_t)slot, p->x, p->y, tx, ty, wp->speed, wp->range, damage, wp->fx)) return;
    p->reload = wp->reload;
    if (wp->fx == FX_SPREAD) {
        // Cajado: mais dois tiros, um de cada lado. Mirar em (4 * dx - dy, 4 * dy + dx)
        // abre uns 14 graus sem precisar de seno nem cosseno.
        int dx = tx - p->x, dy = ty - p->y;
        spawn_shot((uint8_t)slot, p->x, p->y, p->x + 4 * dx - dy, p->y + 4 * dy + dx, wp->speed, wp->range, damage, wp->fx);
        spawn_shot((uint8_t)slot, p->x, p->y, p->x + 4 * dx + dy, p->y + 4 * dy - dx, wp->speed, wp->range, damage, wp->fx);
    }
}

// Dente do Megalodon: quem acertou conserta o próprio barco com parte do dano.
static void leech(int shooter, int dmg) {
    if (shooter < 0) return;
    player_t *p = &g.players[shooter];
    profile_t *pr = prof(p);
    int heal = dmg * LEECH_PCT / 100;
    if (heal < 1) heal = 1;
    if (heal > pr->damage) heal = pr->damage;
    if (!heal) return;
    pr->damage = (uint16_t)(pr->damage - heal);
    update_stats(p); // saiu de "avariado" se estava
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

// Morte por tiro ou mordida: o porão vira uma bag, cai a coroa, cai o saque que o barco
// carregava e o jogador renasce no porto com o barco inteiro.
// `killer` é o slot de quem atirou, -1 se ele saiu do jogo, ou
// KILLER_PREDATOR + tipo quando foi um bicho.
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
    p->grabbed = 0;
    find_spawn(p);
    update_stats(p);
    storage_save(p->profile, pr);
    send_event(slot, EV_DIED, (uint8_t)killer, (uint16_t)fish, 0);
    send_inventory(slot);

    if (killer >= 0 && killer < MAX_PLAYERS && g.players[killer].state == SLOT_PLAYING) {
        prof(&g.players[killer])->kills++;
        send_event(killer, EV_KILL, (uint8_t)slot, 0, 0);
    }
    send_marks(-1);
}

static void hit(int target, shot_t *sh) {
    player_t *t = &g.players[target];
    profile_t *pr = prof(t);
    int dmg = sh->damage;
    int shooter = sh->owner < MAX_PLAYERS && g.players[sh->owner].state == SLOT_PLAYING ? sh->owner : -1;
    pr->damage = (uint16_t)(pr->damage + dmg > hp_max(t) ? hp_max(t) : pr->damage + dmg);
    if (sh->fx == FX_LEECH) leech(shooter, dmg);
    if (sh->fx == FX_GRAB) t->grabbed = GRAB_TICKS, t->vel = 0;
    send_event(target, EV_HURT, sh->owner, (uint16_t)dmg, (uint32_t)hp(t));
    if (shooter >= 0) send_event(shooter, EV_HIT, (uint8_t)target, (uint16_t)dmg, (uint32_t)hp(t));
    // Vida 0 por tiro é morte (na costa continua sendo só avaria).
    if (hp(t) == 0) die(target, sh->owner >= KILLER_PREDATOR ? sh->owner : shooter);
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

// Tiro num bicho. Quem afunda um tubarão leva a recompensa; o prêmio do
// navio fantasma é dividido em update_world, por isso aqui só se anota o dano.
static void hit_predator(predator_t *pd, const shot_t *sh) {
    int shooter = g.players[sh->owner].state == SLOT_PLAYING ? sh->owner : -1;
    pd->hp = (int16_t)(pd->hp - sh->damage);
    if (sh->fx == FX_LEECH) leech(shooter, sh->damage);
    if (sh->fx == FX_GRAB) pd->grabbed = GRAB_TICKS;
    if (pd->kind == PRED_GHOST && shooter >= 0) g.ghost_dmg[shooter] += sh->damage;
    if (pd->hp > 0) {
        if (shooter >= 0) send_event(shooter, EV_PREDATOR_HIT, pd->kind, sh->damage, (uint32_t)pd->hp);
        return;
    }
    pd->alive = 0;
    if (shooter < 0 || pd->kind != PRED_SHARK) return;
    profile_t *pr = prof(&g.players[shooter]);
    pr->money += SHARK_BOUNTY_CENTS;
    send_event(shooter, EV_PREDATOR_KILL, pd->kind, 0, SHARK_BOUNTY_CENTS);
    send_rank(-1);
}

static int hit_boss(const shot_t *sh, int cx, int cy);

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
            // Tiro de bicho não acerta bicho nem boss.
            if (sh->alive && sh->owner < MAX_PLAYERS && hit_boss(sh, cx, cy)) { sh->alive = 0; break; }
            for (int k = 0; sh->alive && sh->owner < MAX_PLAYERS && k < MAX_PREDATORS; k++) {
                predator_t *pd = &g.predators[k];
                if (!pd->alive || pd->y != cy || pd->x < cx - 1 || pd->x > cx + 1) continue;
                hit_predator(pd, sh);
                sh->alive = 0;
            }
        }
    }
}

// --- predadores ---------------------------------------------------------------

// O tubarão não entra no raso; o navio fantasma vai em qualquer água.
// Nenhum dos dois entra em zona segura.
static int pred_water(const predator_t *pd, int x, int y) {
    tile_t t = tile(x, y);
    int ok = pd->kind == PRED_SHARK ? t == TILE_DEEP || t == TILE_WATER : tile_is_water(t);
    return ok && !in_safe_zone(x, y);
}

// Presa: barco no jogo, com pouca vida e fora da zona segura.
static int is_prey(const player_t *p) {
    return p->state == SLOT_PLAYING && hp(p) < SHARK_WEAK_HP && !in_safe_zone(p->x, p->y);
}

// Um passo de uma célula; na diagonal, desliza pela beira como o barco.
static int pred_step(predator_t *pd, int dx, int dy) {
    if (pd->grabbed) return 0; // preso pelo Tentáculo
    if (pred_water(pd, pd->x + dx, pd->y + dy)) return pd->x += dx, pd->y += dy, 1;
    if (dx && dy && pred_water(pd, pd->x + dx, pd->y)) return pd->x += dx, 1;
    if (dx && dy && pred_water(pd, pd->x, pd->y + dy)) return pd->y += dy, 1;
    return 0;
}

// Um tubarão novo na bolha do barco p.
static void spawn_shark(const player_t *p) {
    for (int i = 0; i < MAX_PREDATORS; i++) {
        predator_t *pd = &g.predators[i];
        if (pd->alive) continue;
        for (int tries = 0; tries < 200; tries++) {
            int x, y;
            cell_near(p->x, p->y, &x, &y);
            if (tile(x, y) != TILE_DEEP || !far_from(x, y, PORT_X, PORT_Y, 50) || in_safe_zone(x, y)) continue;
            *pd = (predator_t){.alive = 1, .kind = PRED_SHARK, .x = x, .y = y, .hp = SHARK_HP,
                               .target = -1, .heading = (uint8_t)rnd_below(8)};
            return;
        }
        return;
    }
}

static void bite(int slot, const predator_t *pd) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    pr->damage = (uint16_t)(pr->damage + SHARK_BITE > hp_max(p) ? hp_max(p) : pr->damage + SHARK_BITE);
    update_stats(p);
    send_event(slot, EV_BITTEN, pd->kind, SHARK_BITE, (uint32_t)hp(p));
    if (hp(p) == 0) die(slot, KILLER_PREDATOR + pd->kind);
}

// Navio fantasma: chega perto do barco mais próximo (forte ou fraco) e atira.
static void update_ghost(predator_t *pd) {
    if (pd->target >= 0) {
        const player_t *t = &g.players[pd->target];
        if (t->state != SLOT_PLAYING || in_safe_zone(t->x, t->y) || far_from(t->x, t->y, pd->x, pd->y, GHOST_SENSE * 2))
            pd->target = -1;
    }
    if (pd->target < 0 && g.tick % TICK_HZ == 0) {
        int64_t best = GHOST_SENSE * GHOST_SENSE + 1;
        for (int t = 0; t < MAX_PLAYERS; t++) {
            const player_t *p = &g.players[t];
            int dx = p->x - pd->x, dy = p->y - pd->y;
            if (p->state == SLOT_PLAYING && !in_safe_zone(p->x, p->y) && dist2(dx, dy) < best)
                best = dist2(dx, dy), pd->target = (int8_t)t;
        }
    }
    const player_t *prey = pd->target >= 0 ? &g.players[pd->target] : 0;
    pd->move_budget += GHOST_SPEED;
    while (pd->move_budget >= 10) {
        pd->move_budget -= 10;
        if (prey && far_from(prey->x, prey->y, pd->x, pd->y, GHOST_KEEP)) {
            pred_step(pd, (prey->x > pd->x) - (prey->x < pd->x), (prey->y > pd->y) - (prey->y < pd->y));
        } else if (!prey && (!pred_step(pd, DIR_X[pd->heading], DIR_Y[pd->heading]) || rnd_below(10) == 0)) {
            pd->heading = (uint8_t)rnd_below(8);
        }
    }
    // `bite` serve de recarga do canhão.
    if (prey && !pd->bite && !far_from(prey->x, prey->y, pd->x, pd->y, GHOST_RANGE + 1)) {
        if (spawn_shot(KILLER_PREDATOR + PRED_GHOST, pd->x, pd->y, prey->x, prey->y, GHOST_SHOT_SPEED, GHOST_RANGE + 2,
                       GHOST_DAMAGE, FX_NONE))
            pd->bite = GHOST_RELOAD;
    }
}

static void update_predators(void) {
    for (int i = 0; i < MAX_PREDATORS; i++) {
        predator_t *pd = &g.predators[i];
        if (!pd->alive) continue;
        if (pd->bite) pd->bite--;
        if (pd->grabbed) pd->grabbed--;
        if (pd->kind == PRED_GHOST) { update_ghost(pd); continue; }
        // Ficou longe de todo mundo: some, e a vaga nasce de novo perto de alguém.
        if (g.tick % TICK_HZ == 0 && far_from_boats(pd->x, pd->y)) { pd->alive = 0; continue; }
        // A presa escapa se consertou o barco, entrou na zona segura ou fugiu longe.
        if (pd->target >= 0) {
            const player_t *t = &g.players[pd->target];
            if (!is_prey(t) || far_from(t->x, t->y, pd->x, pd->y, SHARK_SENSE * 2)) pd->target = -1;
        }
        // Sem presa, procura uma vez por segundo: o barco fraco mais perto.
        if (pd->target < 0 && g.tick % TICK_HZ == 0) {
            int64_t best = SHARK_SENSE * SHARK_SENSE + 1;
            for (int t = 0; t < MAX_PLAYERS; t++) {
                const player_t *p = &g.players[t];
                int dx = p->x - pd->x, dy = p->y - pd->y;
                if (is_prey(p) && dist2(dx, dy) < best) best = dist2(dx, dy), pd->target = (int8_t)t;
            }
        }
        const player_t *prey = pd->target >= 0 ? &g.players[pd->target] : 0;
        pd->move_budget += prey ? SHARK_SPEED_CHASE : SHARK_SPEED_WANDER;
        while (pd->move_budget >= 10) {
            pd->move_budget -= 10;
            if (prey) {
                int dx = (prey->x > pd->x) - (prey->x < pd->x), dy = (prey->y > pd->y) - (prey->y < pd->y);
                if (iabs(prey->x - pd->x) <= 1 && iabs(prey->y - pd->y) <= 1) break; // já está colado
                pred_step(pd, dx, dy);
            } else if (!pred_step(pd, DIR_X[pd->heading], DIR_Y[pd->heading]) || rnd_below(10) == 0) {
                pd->heading = (uint8_t)rnd_below(8); // bateu na beira ou cansou: muda de rumo
            }
        }
        if (prey && !pd->bite && iabs(prey->x - pd->x) <= 1 && iabs(prey->y - pd->y) <= 1) {
            pd->bite = SHARK_BITE_EVERY;
            bite(pd->target, pd);
        }
    }
    // A cada SHARK_RESPAWN, cada barco com menos tubarões na bolha do que o
    // bioma dele pede (BIOME_SHARKS) ganha mais um.
    if (g.tick >= g.next_shark) {
        g.next_shark = g.tick + SHARK_RESPAWN;
        for (int t = 0; t < MAX_PLAYERS; t++) {
            const player_t *p = &g.players[t];
            if (p->state != SLOT_PLAYING) continue;
            int n = 0;
            for (int i = 0; i < MAX_PREDATORS; i++) {
                const predator_t *pd = &g.predators[i];
                n += pd->alive && pd->kind == PRED_SHARK && in_bubble(p, pd->x, pd->y);
            }
            if (n < BIOME_SHARKS[biome_at(p->x, p->y)]) spawn_shark(p);
        }
    }
}

// --- eventos do mundo ----------------------------------------------------------

static void event_all(uint8_t kind, uint8_t species, uint16_t count, uint32_t value) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) send_event(i, kind, species, count, value);
}

// A célula está dentro do evento `kind`? Zona segura nunca está.
// Catálogo novo nos mascates de todas as vilas: épicos de qualquer tipo
// (menos as armas dos bosses), com encanto +1 ou +2.
static void new_village_catalog(village_t *v) {
    for (int k = 0; k < PEDDLER_ITEMS; k++) {
        uint8_t base = (uint8_t)(1 + rnd_below(ITEM_DROP_COUNT - 1));
        v->offer[k] = item_in_rarity(base, RARITY_EPIC, RARITY_EPIC, rnd());
        v->offer_enchant[k] = (uint8_t)(1 + rnd_below(2));
    }
    // Encomenda: uma espécie que não está saturada no mercado.
    uint8_t fresh[FISH_COUNT], n = 0;
    for (int sp = 0; sp < FISH_COUNT; sp++)
        if (price_cents(sp) * 100 >= (uint32_t)SPECIES[sp].price_cents * QUEST_FRESH_PCT) fresh[n++] = (uint8_t)sp;
    uint8_t sp = n ? fresh[rnd_below(n)] : (uint8_t)rnd_below(FISH_COUNT);
    int count = QUEST_VALUE / (int)SPECIES[sp].price_cents;
    v->quest_species = sp;
    v->quest_count = (uint8_t)(count < QUEST_MIN ? QUEST_MIN : count > QUEST_MAX ? QUEST_MAX : count);
    memset(v->quest_done, 0, sizeof v->quest_done);
}

static void new_catalog(void) {
    for (int i = 0; i < g.village_count; i++) new_village_catalog(&g.villages[i]);
    g.catalog_ends = g.tick + CATALOG_TIME;
}

static void update_catalog(void) {
    if (g.tick < g.catalog_ends) return;
    new_catalog();
    event_all(EV_CATALOG, FISH_NONE, CATALOG_TIME / TICK_HZ / 60, 0); // count = minutos até a próxima
    send_catalogs();
}

static int in_event(uint8_t kind, int x, int y) {
    if (g.event.kind != kind || in_safe_zone(x, y)) return 0;
    int dx = x - g.event.x, dy = y - g.event.y;
    return dist2(dx, dy) <= g.event.r * g.event.r;
}

// Um lugar no mar aberto, longe do porto, com o tile pedido no centro.
static int event_spot(int deep_only, int *ox, int *oy) {
    for (int tries = 0; tries < 500; tries++) {
        int x, y;
        random_cell(&x, &y);
        tile_t t = tile(x, y);
        if (deep_only ? t != TILE_DEEP : !tile_is_water(t)) continue;
        if (!far_from(x, y, PORT_X, PORT_Y, EVENT_MIN_DIST) || in_safe_zone(x, y)) continue;
        *ox = x, *oy = y;
        return 1;
    }
    return 0;
}

// Maré de lulas: cardumes de lula-gigante aparecem juntos em água funda, e a
// região fica com o estoque cheio.
static void squid_tide(void) {
    int r = g.event.r;
    for (int i = 0, made = 0; i < 300 && made < SQUID_SCHOOLS; i++) {
        int x = g.event.x + (int)rnd_below((uint32_t)(2 * r + 1)) - r;
        int y = g.event.y + (int)rnd_below((uint32_t)(2 * r + 1)) - r;
        if (!in_event(WORLD_SQUID, x, y) || tile(x, y) != TILE_DEEP) continue;
        g.schools[made++] = (school_t){.alive = 1, .x = (uint16_t)x, .y = (uint16_t)y, .r = (uint8_t)(3 + rnd_below(3)),
                                       .species = FISH_LULA, .expires = g.event.ends};
        stock_cell_t *c = stock_find(x, y, 0);
        if (c) c->stock = STOCK_MAX;
    }
    send_marks(-1);
}

// O navio fantasma ocupa uma vaga livre em `predators`. -1 se não tem vaga.
static int spawn_ghost(int x, int y) {
    for (int i = 0; i < MAX_PREDATORS; i++) {
        predator_t *pd = &g.predators[i];
        if (pd->alive) continue;
        *pd = (predator_t){.alive = 1, .kind = PRED_GHOST, .x = x, .y = y, .hp = GHOST_HP,
                           .target = -1, .heading = (uint8_t)rnd_below(8)};
        memset(g.ghost_dmg, 0, sizeof g.ghost_dmg);
        return i;
    }
    return -1;
}

static void start_event(uint8_t kind) {
    int x, y;
    if (!event_spot(kind == WORLD_SQUID, &x, &y)) { g.next_event = g.tick + EVENT_GAP; return; }
    if (kind == WORLD_GHOST && (g.ghost = spawn_ghost(x, y)) < 0) { g.next_event = g.tick + EVENT_GAP; return; }
    int dir = (int)rnd_below(8);
    g.event = (world_event_t){
        .kind = kind,
        .x = x,
        .y = y,
        .r = kind == WORLD_STORM ? STORM_R : kind == WORLD_SQUID ? SQUID_R : GHOST_R,
        .dx = DIR_X[dir],
        .dy = DIR_Y[dir],
        .ends = g.tick + (kind == WORLD_STORM ? STORM_TIME : kind == WORLD_SQUID ? SQUID_TIME : GHOST_TIME),
    };
    if (kind == WORLD_SQUID) squid_tide();
    // `count` = segundos que dura, `value` = onde (x nos 16 bits de baixo, y nos de cima).
    event_all(EV_WORLD_START, kind, (uint16_t)((g.event.ends - g.tick) / TICK_HZ), (uint32_t)x | (uint32_t)y << 16);
}

static void end_event(void) {
    uint8_t kind = g.event.kind;
    // Acabou o tempo: o navio fantasma some na neblina.
    if (kind == WORLD_GHOST && g.ghost >= 0) g.predators[g.ghost].alive = 0;
    g.ghost = -1;
    g.event = (world_event_t){0};
    g.next_event = g.tick + EVENT_GAP + rnd_below(EVENT_GAP);
    event_all(EV_WORLD_END, kind, 0, 0);
}

// Afundaram o navio fantasma: $300 divididos pelo dano de cada um, e todo
// mundo que acertou ganha um baú.
static void ghost_sunk(void) {
    uint32_t total = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) total += g.ghost_dmg[i];
    for (int i = 0; i < MAX_PLAYERS && total; i++) {
        if (!g.ghost_dmg[i] || g.players[i].state != SLOT_PLAYING) continue;
        uint32_t share = (uint32_t)((uint64_t)GHOST_BOUNTY_CENTS * g.ghost_dmg[i] / total);
        profile_t *pr = prof(&g.players[i]);
        pr->money += share;
        storage_save(g.players[i].profile, pr);
        send_event(i, EV_PREDATOR_KILL, PRED_GHOST, g.ghost_dmg[i], share);
        give_random_item(i);
    }
    g.ghost = -1; // já afundou: end_event não precisa sumir com ele
    end_event();
    send_rank(-1);
}

static void update_world(void) {
    if (g.event.kind && g.tick >= g.event.ends) end_event();
    if (g.event.kind == WORLD_GHOST && !g.predators[g.ghost].alive) ghost_sunk();
    if (!g.event.kind && g.tick >= g.next_event) start_event((uint8_t)(WORLD_STORM + rnd_below(3)));
    // O anúncio do navio fantasma acompanha o navio.
    if (g.event.kind == WORLD_GHOST) g.event.x = g.predators[g.ghost].x, g.event.y = g.predators[g.ghost].y;
    // A tempestade anda com o vento, sem sair do mapa.
    if (g.event.kind == WORLD_STORM && g.tick % STORM_DRIFT_EVERY == 0) {
        int nx = g.event.x + g.event.dx, ny = g.event.y + g.event.dy;
        if (nx < 0 || nx >= MAP_W) g.event.dx = (int8_t)-g.event.dx, nx = g.event.x;
        if (ny < 0 || ny >= MAP_H) g.event.dy = (int8_t)-g.event.dy, ny = g.event.y;
        g.event.x = nx;
        g.event.y = ny;
    }
}

// Barco dentro da tempestade: o vento empurra e as ondas castigam o casco.
// Como bater na costa, isso só avaria o barco, não afunda.
static void storm(int slot) {
    player_t *p = &g.players[slot];
    if (g.tick % STORM_PUSH_EVERY == 0) {
        int nx = p->x + g.event.dx, ny = p->y + g.event.dy;
        if (tile_is_sailable(tile(nx, ny))) p->x = nx, p->y = ny;
    }
    if (g.tick % STORM_HURT_EVERY == 0 && hp(p) > 0) {
        prof(p)->damage++;
        update_stats(p);
        if (hp(p) == 0) send_event(slot, EV_BROKEN, FISH_NONE, 0, 0);
    }
}

// --- clima ----------------------------------------------------------------------

// Na hora marcada o clima muda: do limpo para chuva ou neblina (meio a meio)
// e de volta para o limpo. Todo mundo fica sabendo pelo EV_WEATHER.
static void update_weather(void) {
    weather_t *w = &g.weather;
    if (g.tick < w->ends) return;
    if (w->kind == WEATHER_CLEAR) {
        w->kind = (uint8_t)(WEATHER_RAIN + rnd_below(2));
        w->ends = g.tick + WEATHER_TIME + rnd_below(WEATHER_TIME);
    } else {
        w->kind = WEATHER_CLEAR;
        w->ends = g.tick + WEATHER_GAP + rnd_below(WEATHER_GAP);
    }
    event_all(EV_WEATHER, w->kind, (uint16_t)((w->ends - g.tick) / TICK_HZ), 0);
}

// --- bosses -------------------------------------------------------------------

// O Megalodon só nada no fundo; nenhum boss entra em zona segura.
static int boss_water(int x, int y) {
    tile_t t = tile(x, y);
    int ok = g.boss.kind == BOSS_CTHULHU ? tile_is_water(t) : t == TILE_DEEP;
    return ok && !in_safe_zone(x, y);
}

static int boss_step(int dx, int dy) {
    boss_t *b = &g.boss;
    if (b->grabbed) return 0;
    if (boss_water(b->x + dx, b->y + dy)) return b->x += dx, b->y += dy, 1;
    if (dx && dy && boss_water(b->x + dx, b->y)) return b->x += dx, 1;
    if (dx && dy && boss_water(b->x, b->y + dy)) return b->y += dy, 1;
    return 0;
}

// Anda `speed` células por segundo atrás do alvo, ou passeando se não tem.
static void boss_swim(const player_t *prey, int speed) {
    boss_t *b = &g.boss;
    b->move_budget += speed;
    while (b->move_budget >= 10) {
        b->move_budget -= 10;
        if (prey) {
            if (iabs(prey->x - b->x) <= 1 && iabs(prey->y - b->y) <= 1) break; // já está em cima
            boss_step((prey->x > b->x) - (prey->x < b->x), (prey->y > b->y) - (prey->y < b->y));
        } else if (!boss_step(DIR_X[b->heading], DIR_Y[b->heading]) || rnd_below(10) == 0) {
            b->heading = (uint8_t)rnd_below(8);
        }
    }
}

// Dano do boss num barco. Igual tiro: com vida 0, afunda.
static void boss_hurt(int slot, int dmg) {
    player_t *p = &g.players[slot];
    profile_t *pr = prof(p);
    pr->damage = (uint16_t)(pr->damage + dmg > hp_max(p) ? hp_max(p) : pr->damage + dmg);
    update_stats(p);
    send_event(slot, EV_HURT, (uint8_t)(KILLER_BOSS + g.boss.kind), (uint16_t)dmg, (uint32_t)hp(p));
    if (hp(p) == 0) die(slot, KILLER_BOSS + g.boss.kind);
}

// O barco mais perto, fora da zona segura, até `range` células. -1 se nenhum.
static int boss_nearest(int range) {
    int64_t best = (int64_t)range * range + 1;
    int found = -1;
    for (int t = 0; t < MAX_PLAYERS; t++) {
        const player_t *p = &g.players[t];
        int dx = p->x - g.boss.x, dy = p->y - g.boss.y;
        if (p->state == SLOT_PLAYING && !in_safe_zone(p->x, p->y) && dist2(dx, dy) < best)
            best = dist2(dx, dy), found = t;
    }
    return found;
}

// Megalodon: nada atrás do barco mais perto e, chegando a 12 células, dá uma
// investida rápida demais para fugir. Se morder, tira 25; depois descansa.
static void update_megalodon(void) {
    boss_t *b = &g.boss;
    if (b->target >= 0) {
        const player_t *t = &g.players[b->target];
        if (t->state != SLOT_PLAYING || in_safe_zone(t->x, t->y) || far_from(t->x, t->y, b->x, b->y, BOSS_SENSE * 2))
            b->target = -1, b->attack = 0;
    }
    if (b->target < 0 && g.tick % TICK_HZ == 0) b->target = (int8_t)boss_nearest(BOSS_SENSE);
    const player_t *prey = b->target >= 0 ? &g.players[b->target] : 0;
    if (prey && !b->attack && !b->rest && !far_from(prey->x, prey->y, b->x, b->y, MEGA_CHARGE_FROM))
        b->attack = MEGA_CHARGE_TIME;
    boss_swim(prey, b->attack ? MEGA_CHARGE_SPEED : MEGA_SPEED);
    if (!b->attack) return;
    b->attack--;
    if (iabs(prey->x - b->x) <= 1 && iabs(prey->y - b->y) <= 1) {
        b->attack = 0;
        b->rest = MEGA_REST;
        boss_hurt(b->target, MEGA_BITE);
    } else if (!b->attack) {
        b->rest = MEGA_REST; // errou: cansou do mesmo jeito
    }
}

// Kraken: fica parado no fundo e, a cada 3 s, os tentáculos agarram os 3
// barcos mais perto (até 12 células): tiram vida, prendem por 2 s e puxam
// o barco para perto dele. Quem tem arma de longe bate sem ser pego.
static void update_kraken(void) {
    boss_t *b = &g.boss;
    if (b->attack) b->attack--;
    if (b->rest) return;
    b->rest = KRAKEN_EVERY;
    uint8_t taken[MAX_PLAYERS] = {0};
    for (int arm = 0; arm < KRAKEN_ARMS; arm++) {
        int64_t best = KRAKEN_REACH * KRAKEN_REACH + 1;
        int slot = -1;
        for (int t = 0; t < MAX_PLAYERS; t++) {
            const player_t *p = &g.players[t];
            int dx = p->x - b->x, dy = p->y - b->y;
            if (p->state == SLOT_PLAYING && !taken[t] && !in_safe_zone(p->x, p->y) && dist2(dx, dy) < best)
                best = dist2(dx, dy), slot = t;
        }
        if (slot < 0) break;
        taken[slot] = 1;
        b->attack = 5; // o cliente desenha os tentáculos por meio segundo
        player_t *p = &g.players[slot];
        for (int i = 0; i < KRAKEN_PULL; i++) {
            int nx = p->x + ((b->x > p->x) - (b->x < p->x)), ny = p->y + ((b->y > p->y) - (b->y < p->y));
            if (iabs(nx - b->x) <= 1 && iabs(ny - b->y) <= 1) break; // já está colado nele
            if (tile_is_sailable(tile(nx, ny))) p->x = nx, p->y = ny;
        }
        p->grabbed = GRAB_TICKS;
        p->vel = 0;
        boss_hurt(slot, KRAKEN_DAMAGE);
    }
}

// Cthulhu: vem devagar atrás do barco mais perto e, a cada 5 s, solta um anel
// de 8 tiros em volta. Quem chega a 15 células dele fica louco (is_mad).
static void update_cthulhu(void) {
    boss_t *b = &g.boss;
    if (b->attack) b->attack--;
    if (b->target >= 0) {
        const player_t *t = &g.players[b->target];
        if (t->state != SLOT_PLAYING || in_safe_zone(t->x, t->y) || far_from(t->x, t->y, b->x, b->y, BOSS_SENSE * 2))
            b->target = -1;
    }
    if (b->target < 0 && g.tick % TICK_HZ == 0) b->target = (int8_t)boss_nearest(BOSS_SENSE);
    boss_swim(b->target >= 0 ? &g.players[b->target] : 0, CTHULHU_SPEED);
    if (b->rest || b->target < 0) return; // sem ninguém por perto, não gasta o anel
    b->rest = CTHULHU_RING_EVERY;
    b->attack = 5;
    for (int d = 0; d < 8; d++)
        spawn_shot(KILLER_BOSS + BOSS_CTHULHU, b->x, b->y, b->x + 8 * DIR_X[d], b->y + 8 * DIR_Y[d], CTHULHU_RING_SPEED,
                   CTHULHU_RING_RANGE, CTHULHU_RING_DAMAGE, FX_NONE);
}

// Loucura do Cthulhu: barco perto dele (e fora da zona segura) tem o leme invertido.
static int is_mad(const player_t *p) {
    return g.boss.kind == BOSS_CTHULHU && !far_from(p->x, p->y, g.boss.x, g.boss.y, CTHULHU_MADNESS_R) &&
           !in_safe_zone(p->x, p->y);
}

// Qual boss vem: Megalodon 45%, Kraken 35%, Cthulhu 20% (o mais raro e mais forte).
static uint8_t pick_boss(void) {
    uint32_t r = rnd_below(100);
    return r < 45 ? BOSS_MEGALODON : r < 80 ? BOSS_KRAKEN : BOSS_CTHULHU;
}

// Lugar para o boss nascer: água do tipo que ele gosta, longe do porto.
static int boss_spot(int *ox, int *oy) {
    for (int tries = 0; tries < 1000; tries++) {
        int x, y;
        random_cell(&x, &y);
        if (!boss_water(x, y) || !far_from(x, y, PORT_X, PORT_Y, BOSS_MIN_DIST)) continue;
        *ox = x, *oy = y;
        return 1;
    }
    return 0;
}

static void start_boss(uint8_t kind) {
    uint8_t online = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) online += g.players[i].state == SLOT_PLAYING;
    g.next_boss = g.tick + BOSS_GAP + rnd_below(BOSS_GAP);
    if (!online) { g.next_boss = g.tick + 60 * TICK_HZ; return; } // ninguém para enfrentar: espera
    g.boss = (boss_t){.kind = kind, .target = -1};
    int x, y;
    if (!boss_spot(&x, &y)) { g.boss.kind = BOSS_NONE; return; }
    const boss_info_t *info = &BOSSES[kind];
    g.boss.x = x;
    g.boss.y = y;
    g.boss.hp = g.boss.hp_max = (int16_t)(info->hp_base + info->hp_per_player * online);
    g.boss.heading = (uint8_t)rnd_below(8);
    g.boss.ends = g.tick + BOSS_TIME;
    event_all(EV_BOSS_START, kind, BOSS_TIME / TICK_HZ, (uint32_t)x | (uint32_t)y << 16);
}

// Prêmio: o dinheiro é dividido pelo dano. A arma única vai para quem mais
// bateu (com a bolsa cheia, passa para o seguinte). Quem deu pelo menos 10%
// do dano ganha um baú.
static void boss_killed(void) {
    boss_t *b = &g.boss;
    const boss_info_t *info = &BOSSES[b->kind];
    uint32_t total = 0;
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) total += b->dmg[i];
    int mvp = -1;
    uint8_t got[MAX_PLAYERS] = {0}; // já tentou dar a arma para ele
    item_t weapon = item_new(info->weapon, rnd());
    while (total) {
        int best = -1;
        for (int i = 0; i < MAX_PLAYERS; i++)
            if (g.players[i].state == SLOT_PLAYING && b->dmg[i] && !got[i] && (best < 0 || b->dmg[i] > b->dmg[best]))
                best = i;
        if (best < 0) break;
        got[best] = 1;
        if (bag_add(prof(&g.players[best]), weapon) < 0) {
            send_event(best, EV_BAG_FULL, FISH_NONE, 0, 0);
            continue;
        }
        mvp = best;
        send_event(best, EV_ITEM, weapon.base, item_info(weapon).rarity, 0);
        break;
    }
    for (int i = 0; i < MAX_PLAYERS && total; i++) {
        if (!b->dmg[i] || g.players[i].state != SLOT_PLAYING) continue;
        uint32_t share = (uint32_t)((uint64_t)info->bounty_cents * b->dmg[i] / total);
        profile_t *pr = prof(&g.players[i]);
        pr->money += share;
        send_event(i, EV_BOSS_REWARD, b->kind, b->dmg[i], share);
        if (i != mvp && (uint32_t)b->dmg[i] * 100 >= total * BOSS_HELPER_PCT) give_random_item(i);
        if (i == mvp) send_inventory(i);
        storage_save(g.players[i].profile, pr);
    }
    // `count` 1 = afundou; `value` = slot de quem levou a arma + 1 (0 = ninguém).
    event_all(EV_BOSS_END, b->kind, 1, (uint32_t)(mvp + 1));
    g.boss = (boss_t){0};
    send_rank(-1);
}

// Tiro de jogador passando por (cx, cy): acerta se cair dentro do corpo do boss.
static int hit_boss(const shot_t *sh, int cx, int cy) {
    boss_t *b = &g.boss;
    if (!b->kind || b->hp <= 0) return 0;
    int r = BOSSES[b->kind].r, dx = cx - b->x, dy = cy - b->y;
    if (dist2(dx, dy) > r * r) return 0;
    int shooter = g.players[sh->owner].state == SLOT_PLAYING ? sh->owner : -1;
    int dmg = sh->damage < b->hp ? sh->damage : b->hp;
    b->hp = (int16_t)(b->hp - dmg);
    if (shooter >= 0) {
        b->dmg[shooter] = (uint16_t)(b->dmg[shooter] + dmg);
        send_event(shooter, EV_BOSS_HIT, b->kind, (uint16_t)dmg, (uint32_t)b->hp);
    }
    if (sh->fx == FX_LEECH) leech(shooter, dmg);
    if (sh->fx == FX_GRAB) b->grabbed = GRAB_TICKS;
    return 1;
}

static void update_boss(void) {
    boss_t *b = &g.boss;
    if (!b->kind) {
        if (g.tick >= g.next_boss) start_boss(pick_boss());
        return;
    }
    if (b->hp <= 0) { boss_killed(); return; }
    if (g.tick >= b->ends) { // ninguém afundou: volta para o fundo
        event_all(EV_BOSS_END, b->kind, 0, 0);
        g.boss = (boss_t){0};
        return;
    }
    if (b->grabbed) b->grabbed--;
    if (b->rest) b->rest--;
    if (b->kind == BOSS_MEGALODON) update_megalodon();
    if (b->kind == BOSS_KRAKEN) update_kraken();
    if (b->kind == BOSS_CTHULHU) update_cthulhu();
}

static void update_player(int slot) {
    player_t *p = &g.players[slot];
    // Inércia: com tecla apertada acelera 1 célula/s por tick; soltou,
    // o barco desliza e perde 1 célula/s a cada 2 ticks.
    // Louco pelo Cthulhu: o leme responde ao contrário.
    int want = is_mad(p) ? dir_index(-p->dx, -p->dy) : dir_index(p->dx, p->dy);
    if (want >= 0) {
        steer(p, want);
        if (p->vel < p->speed) p->vel++;
    } else if (p->vel > 0 && g.tick % 2 == 0) {
        p->vel--;
    }
    if (p->vel > p->speed) p->vel = p->speed; // tirou o motor, por exemplo
    if (p->grabbed) p->grabbed--, p->vel = 0; // preso pelo Tentáculo: vira, mas não anda
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
    int in_storm = in_event(WORLD_STORM, p->x, p->y);
    if (in_storm) storm(slot);
    prof(p)->x = (uint16_t)p->x;
    prof(p)->y = (uint16_t)p->y;
    prof(p)->map_version = MAP_VERSION;
    pick_up(slot);

    // Produção = % a mais na chance de pescar. +50 de produção: 6% vira 9%.
    int production = p->stats[STAT_PRODUCTION] > -100 ? p->stats[STAT_PRODUCTION] : -100;
    uint32_t passive = (uint32_t)(PASSIVE_CATCH_PCT * 10 * (100 + production) / 100);
    uint32_t school = (uint32_t)(SCHOOL_CATCH_PCT * 10 * (100 + production) / 100);
    if (in_storm) passive = passive * STORM_CATCH_PCT / 100, school = school * STORM_CATCH_PCT / 100;
    if (g.weather.kind == WEATHER_RAIN) passive = passive * RAIN_CATCH_PCT / 100, school = school * RAIN_CATCH_PCT / 100;
    if (moved && chance_permille(with_stock(passive, p->x, p->y))) {
        uint8_t sp = fish_roll(tile(p->x, p->y), biome_at(p->x, p->y), rnd());
        if (sp != FISH_NONE) add_fish(slot, sp, 1);
    }
    for (int i = 0; i < MAX_SCHOOLS; i++) {
        const school_t *s = &g.schools[i];
        int dx = p->x - s->x, dy = p->y - s->y;
        if (s->alive && dist2(dx, dy) <= s->r * s->r && chance_permille(with_stock(school, p->x, p->y)))
            add_fish(slot, s->species, 1 + (int)rnd_below(3));
    }
}

static void update_schools(void) {
    for (int i = 0; i < MAX_SCHOOLS; i++) {
        school_t *s = &g.schools[i];
        // O cardume vai embora no fim da vida ou quando a região fica sem peixe.
        // Também some quando fica longe de todos os barcos, para nascer perto de alguém.
        if (s->alive && (s->expires <= g.tick || stock_get(s->x, s->y) < SCHOOL_MIN_STOCK)) s->alive = 0;
        if (s->alive && g.tick % TICK_HZ == 0 && far_from_boats(s->x, s->y)) s->alive = 0;
    }
    // Completa a bolha de cada barco até a meta do bioma dele. Nada de cardume
    // colado no porto: precisa sair para buscar. E ele nasce mais onde tem
    // mais peixe: numa região com 40% do estoque, só 40% das tentativas valem.
    int free_i = 0;
    for (int t = 0; t < MAX_PLAYERS; t++) {
        const player_t *p = &g.players[t];
        if (p->state != SLOT_PLAYING) continue;
        int have = 0, want = SCHOOLS_PER_BOAT * BIOME_SCHOOL_PCT[biome_at(p->x, p->y)] / 100;
        for (int i = 0; i < MAX_SCHOOLS; i++) have += g.schools[i].alive && in_bubble(p, g.schools[i].x, g.schools[i].y);
        for (int attempts = 300; have < want && attempts > 0; attempts--) {
            while (free_i < MAX_SCHOOLS && g.schools[free_i].alive) free_i++;
            if (free_i == MAX_SCHOOLS) break; // sem vaga
            int x, y;
            cell_near(p->x, p->y, &x, &y);
            int dx = x - PORT_X, dy = y - PORT_Y;
            tile_t t = tile(x, y);
            if (!tile_is_water(t) || dist2(dx, dy) < 30 * 30) continue;
            if (rnd_below(STOCK_MAX) >= (uint32_t)stock_get(x, y)) continue;
            g.schools[free_i] = (school_t){
                .alive = 1,
                .x = (uint16_t)x,
                .y = (uint16_t)y,
                .r = (uint8_t)(2 + rnd_below(4)),
                .species = fish_roll(t, biome_at(x, y), rnd()),
                .expires = g.tick + 600 + rnd_below(1200), // 1 a 3 minutos
            };
            have++;
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

// Crescimento logístico: devagar com pouco peixe (poucos para se reproduzir),
// rápido no meio e devagar de novo perto do máximo. O +1 garante que até uma
// região vazia volta.
// A tabela é refeita só com as regiões que ainda não encheram: as cheias
// saem, e assim ela não cresce à toa.
static void regen_stock(void) {
    static stock_cell_t old[STOCK_SLOTS];
    memcpy(old, g.stock, sizeof old);
    memset(g.stock, 0, sizeof g.stock);
    g.stock_used = 0;
    for (int i = 0; i < STOCK_SLOTS; i++) {
        if (!old[i].used) continue;
        int s = old[i].stock;
        s += 1 + s * (STOCK_MAX - s) / STOCK_GROWTH;
        if (s >= STOCK_MAX) continue;
        stock_cell_t *c = stock_slot(old[i].rx, old[i].ry, 1);
        if (c) c->stock = (uint8_t)s;
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
    // Quem parou num mapa antigo (map_version diferente) volta para o porto:
    // o (x, y) salvo era de outro mapa.
    const profile_t *pr = &g.profiles[index];
    if (pr->map_version == MAP_VERSION && (pr->x || pr->y) && tile_is_sailable(tile(pr->x, pr->y))) {
        p->x = pr->x;
        p->y = pr->y;
    } else {
        find_spawn(p);
    }

    writer_t w = writer(out, sizeof out);
    put_u8(&w, S_WELCOME);
    put_u8(&w, (uint8_t)slot);
    put_u32(&w, g.seed);
    put_u32(&w, MAP_W); // 65536 não cabe num u16
    put_u32(&w, MAP_H);
    put_u8(&w, TICK_HZ);
    put_u8(&w, PROBE_COUNT);
    for (int i = 0; i < PROBE_COUNT; i++) {
        put_u16(&w, (uint16_t)(PORT_RX + PROBES[i][0]));
        put_u16(&w, (uint16_t)(PORT_RY + PROBES[i][1]));
        put_u32(&w, g.probe_sum[i]);
    }
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
    struct_resolve_near(p->x, p->y);
    send_marks(slot);
    send_inventory(slot);
    send_boat(slot);
    send_rank(-1);
    send_event(slot, EV_WELCOME, FISH_NONE, 0, 0);
}

// Um cais na costa: célula de água com terra colada (a ilha) e pelo menos 5
// das 8 vizinhas com água, para o barco conseguir encostar.
static int is_coast(int x, int y) {
    if (!tile_is_water(tile(x, y))) return 0;
    int land = 0, water = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            tile_t t = tile(x + dx, y + dy);
            land += t == TILE_SAND || t == TILE_GRASS;
            water += tile_is_water(t);
        }
    return land && water >= 5;
}

// --- estruturas ---------------------------------------------------------------

// A pedra da sereia: água em volta (5x5) para dar para chegar.
static int mermaid_spot(int x, int y) {
    for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
            if (!tile_is_water(tile(x + dx, y + dy))) return 0;
    return 1;
}

// Cais de vila: na costa e longe do forte do armeiro.
static int village_spot(int x, int y) {
    const safe_zone_t *a = &g.safe_zones[ZONE_ARMORY];
    return is_coast(x, y) && (a->r <= 0 || far_from(x, y, a->x, a->y, ARMORY_SPACING));
}

// Sorteia (pela semente) até achar um lugar bom perto do centro da região.
static int find_spot(int rx, int ry, uint32_t salt, int (*good)(int, int), int *ox, int *oy) {
    for (int i = 0; i < 200; i++) {
        uint32_t h = world_hash(rx * 256 + i, ry, g.seed ^ salt);
        int x = rx * REGION_CELLS + (int)(h % (2 * STRUCT_SEARCH + 1)) - STRUCT_SEARCH;
        int y = ry * REGION_CELLS + (int)((h >> 16) % (2 * STRUCT_SEARCH + 1)) - STRUCT_SEARCH;
        if (good(x, y)) { *ox = x, *oy = y; return 1; }
    }
    return 0;
}

// Gaveta da região na tabela (hash com sondagem linear): a da região, ou a
// vazia onde ela entraria.
static structure_t *struct_slot(int rx, int ry) {
    for (uint32_t i = world_hash(rx, ry, 0x57A7) & (STRUCT_SLOTS - 1);; i = (i + 1) & (STRUCT_SLOTS - 1)) {
        structure_t *s = &g.structures[i];
        if (!s->used || (s->rx == rx && s->ry == ry)) return s;
    }
}

// Região já descoberta? NULL se ainda não.
static const structure_t *struct_get(int rx, int ry) {
    const structure_t *s = struct_slot(rx, ry);
    return s->used ? s : 0;
}

// Descobre o que tem na região (só na primeira vez). Quando a tabela enche,
// tudo recomeça: as estruturas voltam iguais (vêm da semente), só as vilas
// ganham catálogo e encomenda novos.
static void struct_resolve(int rx, int ry) {
    if (rx < 0 || ry < 0 || rx >= REGIONS_W || ry >= REGIONS_H) return;
    structure_t *s = struct_slot(rx, ry);
    if (s->used) return;
    if (g.struct_used >= STRUCT_SLOTS / 2 || g.village_count >= MAX_VILLAGES) {
        memset(g.structures, 0, sizeof g.structures);
        g.struct_used = g.village_count = 0;
        s = struct_slot(rx, ry);
    }
    *s = (structure_t){.rx = (int16_t)rx, .ry = (int16_t)ry, .used = 1, .kind = STRUCT_NONE, .village = -1};
    g.struct_used++;
    if (rx == PORT_RX && ry == PORT_RY) return; // a região do porto já tem o porto
    biome_t b = world_region_biome(rx, ry, g.seed);
    uint32_t h = world_hash(rx, ry, g.seed ^ 0x57A0C);
    int x, y;
    if ((b == BIOME_ISLANDS || b == BIOME_MANGROVE) && h % VILLAGE_CHANCE == 0 &&
        find_spot(rx, ry, 0x7111A, village_spot, &x, &y)) {
        village_t *v = &g.villages[g.village_count];
        *v = (village_t){.x = x, .y = y, .fav = (uint8_t)((h >> 8) % FISH_COUNT)};
        new_village_catalog(v);
        s->kind = STRUCT_VILLAGE, s->x = x, s->y = y, s->village = (int16_t)g.village_count++;
    } else if (b == BIOME_REEF && h % MERMAID_CHANCE == 0 && find_spot(rx, ry, 0x5E7EA, mermaid_spot, &x, &y)) {
        s->kind = STRUCT_MERMAID, s->x = x, s->y = y;
    }
}

// Descobre as 3x3 regiões em volta de (x, y).
static void struct_resolve_near(int x, int y) {
    int rx0 = world_region_of(x), ry0 = world_region_of(y);
    for (int ry = ry0 - 1; ry <= ry0 + 1; ry++)
        for (int rx = rx0 - 1; rx <= rx0 + 1; rx++) struct_resolve(rx, ry);
}

// Forte do armeiro: um só, numa ilha perto do porto, longe das vilas e da
// sereia. Vem da semente do mapa. Se não achar lugar, fica sem forte (r = 0).
static void place_armory(void) {
    safe_zone_t *z = &g.safe_zones[ZONE_ARMORY];
    *z = (safe_zone_t){0, 0, 0};
    for (int i = 0; i < 20000; i++) {
        uint32_t h = world_hash(i, 0xA4E10, g.seed);
        int x = PORT_X + (int)(h % (2 * ARMORY_MAX_DIST + 1)) - ARMORY_MAX_DIST;
        int y = PORT_Y + (int)((h >> 16) % (2 * ARMORY_MAX_DIST + 1)) - ARMORY_MAX_DIST;
        if (!far_from(x, y, PORT_X, PORT_Y, ARMORY_MIN_DIST) || far_from(x, y, PORT_X, PORT_Y, ARMORY_MAX_DIST + 1)) continue;
        if (struct_near(x, y, STRUCT_VILLAGE, ARMORY_SPACING) || struct_near(x, y, STRUCT_MERMAID, ARMORY_SPACING)) continue;
        if (!is_coast(x, y)) continue;
        *z = (safe_zone_t){x, y, ARMORY_SAFE_R};
        return;
    }
}

// --- API --------------------------------------------------------------------

void game_init(uint32_t seed, uint32_t random_seed, game_send_fn send) {
    memset(&g, 0, sizeof g);
    g.seed = seed;
    g.rng = random_seed ? random_seed : 0x9E3779B9u;
    g.send = send;
    chunks_init(seed);
    for (int i = 0; i < PROBE_COUNT; i++)
        g.probe_sum[i] = world_region_checksum(PORT_RX + PROBES[i][0], PORT_RY + PROBES[i][1], seed);
    g.safe_zones[ZONE_PORT] = (safe_zone_t){PORT_X, PORT_Y, PORT_RADIUS + 14};
    // As 5x5 regiões em volta do porto já nascem descobertas: o forte precisa
    // saber onde ficam as vilas e a sereia de perto para não cair em cima.
    for (int ry = PORT_RY - 2; ry <= PORT_RY + 2; ry++)
        for (int rx = PORT_RX - 2; rx <= PORT_RX + 2; rx++) struct_resolve(rx, ry);
    place_armory();
    new_catalog();
    storage_load_all(g.profiles, MAX_PROFILES);
    update_schools();
    g.next_event = EVENT_FIRST + rnd_below(EVENT_FIRST);
    g.ghost = -1;
    g.next_boss = BOSS_FIRST + rnd_below(BOSS_FIRST);
    g.weather.ends = WEATHER_FIRST + rnd_below(WEATHER_FIRST); // começa limpo
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
    g.boss.dmg[slot] = 0; // o próximo a usar o slot não herda o dano no boss
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
    case C_BUY:
        if (len >= 3) buy(slot, data[1], data[2]);
        break;
    case C_DELIVER:
        deliver(slot);
        break;
    }
}

void game_save_all(void) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) storage_save(g.players[i].profile, prof(&g.players[i]));
}

size_t game_chunks(void) { return chunks_count(); }

int game_online(void) {
    int n = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) n += g.players[i].state == SLOT_PLAYING;
    return n;
}

void game_tick(void) {
    g.tick++;
    if (g.tick % STOCK_REGEN_EVERY == 0) regen_stock();
    update_world();
    update_weather();
    update_catalog();
    update_schools();
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) update_player(i);
    
    update_shots();
    update_predators();
    update_boss();
    update_drops();
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (g.players[i].state == SLOT_PLAYING) send_state(i);

    if (g.tick % TICK_HZ == 0) {
        for (int i = 0; i < MAX_PLAYERS; i++) // vilas e sereias perto de cada barco
            if (g.players[i].state == SLOT_PLAYING) struct_resolve_near(g.players[i].x, g.players[i].y);
        for (int i = 0; i < FISH_COUNT; i++) g.pressure[i] = g.pressure[i] * 99 / 100;
        send_prices(-1);
        send_marks(-1);
        send_catalogs(); // a contagem até a troca anda
    }
    if (g.tick % 50 == 0) send_rank(-1);
    if (g.tick % SAVE_EVERY == 0) game_save_all();
}
