// Gera muitos itens e mostra se as raridades batem com o planejado.
// Linux: cc -I../components/game/include test_items.c ../components/game/items.c -o test_items
// Windows: cl /nologo /utf-8 /I..\components\game\include test_items.c ..\components\game\items.c
#include <stdio.h>
#include <string.h>
#include "items.h"

static const char *RARITY[] = {"comum", "raro", "épico", "lendário"};
static const char *STAT[] = {"velocidade", "vida", "dano", "produção", "sorte"};

static void print_item(item_t it) {
    item_info_t info = item_info(it);
    printf("%-18s %-9s", ITEM_BASES[it.base].name, RARITY[info.rarity]);
    for (int i = 0; i < STAT_COUNT; i++)
        if (info.stats[i]) printf(" %+d %s", info.stats[i], STAT[i]);
    for (int i = 0; i < MAX_AFFIXES && info.affix[i] != AFFIX_NONE; i++) printf("  [%s]", AFFIXES[info.affix[i]].name);
    printf("\n");
}

int main(void) {
    int count[RARITY_COUNT] = {0};
    int ok = 1;
    const int N = 100000;
    for (int i = 0; i < N; i++) {
        item_t it = item_new((uint8_t)(1 + i % (ITEM_BASE_COUNT - 1)), (uint32_t)i * 2654435761u);
        item_info_t a = item_info(it), b = item_info(it);
        if (memcmp(&a, &b, sizeof a) != 0) ok = 0; // mesma semente, mesmo item
        count[a.rarity]++;
    }
    printf("Raridades em %d itens:\n", N);
    for (int i = 0; i < RARITY_COUNT; i++) printf("  %-9s %5.1f%%\n", RARITY[i], 100.0 * count[i] / N);
    printf("\nAlguns exemplos:\n");
    for (uint32_t i = 1; i <= 12; i++) print_item(item_new((uint8_t)(1 + i % (ITEM_BASE_COUNT - 1)), i * 7919u));
    puts(ok ? "\nMesma semente gera sempre o mesmo item." : "\nERRO: item mudou com a mesma semente!");
    return ok ? 0 : 1;
}