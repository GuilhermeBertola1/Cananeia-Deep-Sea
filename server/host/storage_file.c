// Perfis do servidor de teste: um arquivo binário com MAX_PROFILES registros.
#include <stdio.h>
#include <string.h>
#include "storage.h"

static const char *PATH = "perfis.bin";

int storage_load_all(profile_t *out, int max) {
    memset(out, 0, sizeof(profile_t) * max);
    FILE *f = fopen(PATH, "rb");
    if (!f) return 0;
    size_t n = fread(out, sizeof(profile_t), max, f);
    fclose(f);
    int used = 0;
    for (size_t i = 0; i < n; i++) used += out[i].used;
    return used;
}

void storage_save(int index, const profile_t *p) {
    FILE *f = fopen(PATH, "r+b");
    if (!f) f = fopen(PATH, "w+b");
    if (!f) return;
    fseek(f, (long)(index * sizeof(profile_t)), SEEK_SET);
    fwrite(p, sizeof(profile_t), 1, f);
    fclose(f);
}
