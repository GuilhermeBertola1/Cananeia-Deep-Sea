// Perfis do servidor de teste: um arquivo binário com um cabeçalho e depois
// MAX_PROFILES registros.
#include <stdio.h>
#include <string.h>
#include "storage.h"

static const char *PATH = "perfis.bin";

// O cabeçalho guarda o tamanho de profile_t. Quando o perfil ganha um campo
// novo no fim, o tamanho cresce: o arquivo antigo é lido com o tamanho velho,
// os campos novos vêm zerados e o arquivo é regravado no formato novo.
typedef struct {
    char magic[4]; // "CDS\0"
    uint32_t record_size;
} header_t;

static const header_t HEADER = {{'C', 'D', 'S', 0}, sizeof(profile_t)};

static void save_all(const profile_t *profiles, int max) {
    FILE *f = fopen(PATH, "wb");
    if (!f) return;
    fwrite(&HEADER, sizeof HEADER, 1, f);
    fwrite(profiles, sizeof(profile_t), max, f);
    fclose(f);
}

int storage_load_all(profile_t *out, int max) {
    memset(out, 0, sizeof(profile_t) * max);
    FILE *f = fopen(PATH, "rb");
    if (!f) return 0;
    header_t h;
    int ok = fread(&h, sizeof h, 1, f) == 1 && memcmp(h.magic, HEADER.magic, 4) == 0;
    if (!ok || h.record_size > sizeof(profile_t) || h.record_size < PROFILE_V03_SIZE) {
        fclose(f);
        printf("perfis.bin é de outra versão do jogo: começando sem perfis.\n");
        remove(PATH);
        return 0;
    }
    int n = 0;
    while (n < max && fread(&out[n], h.record_size, 1, f) == 1) {
        if (h.record_size < sizeof(profile_t)) profile_upgrade(&out[n], h.record_size);
        n++;
    }
    fclose(f);
    if (h.record_size < sizeof(profile_t)) {
        printf("perfis.bin era de uma versão antiga: perfis convertidos.\n");
        save_all(out, max);
    }
    int used = 0;
    for (int i = 0; i < n; i++) used += out[i].used;
    return used;
}

void storage_save(int index, const profile_t *p) {
    FILE *f = fopen(PATH, "r+b");
    if (!f) {
        f = fopen(PATH, "w+b");
        if (!f) return;
        fwrite(&HEADER, sizeof HEADER, 1, f);
    }
    fseek(f, (long)(sizeof HEADER + index * sizeof(profile_t)), SEEK_SET);
    fwrite(p, sizeof(profile_t), 1, f);
    fclose(f);
}