// Perfis só na memória, para a simulação sem rede (sim.c): nada é lido nem
// gravado no disco, então rodar a simulação nunca mexe no perfis.bin.
#include "storage.h"

int storage_load_all(profile_t *out, int max) {
    memset(out, 0, sizeof(profile_t) * (size_t)max);
    return 0;
}

void storage_save(int index, const profile_t *p) {
    (void)index;
    (void)p;
}
