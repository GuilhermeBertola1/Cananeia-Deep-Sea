// Perfis do servidor de teste: um arquivo binário com um cabeçalho e depois
// MAX_PROFILES registros.
#include <stdio.h>
#include <string.h>
#include "storage.h"

static const char *PATH = "perfis.bin";

// O cabeçalho guarda o tamanho de profile_t. Quando o perfil ganha um campo
// novo, o tamanho muda, e o arquivo antigo é descartado em vez de cair com os
// bytes nos campos errados.
typedef struct {
    char magic[4]; // "CDS\0"
    uint32_t record_size;
} header_t;

static const header_t HEADER = {{'C', 'D', 'S', 0}, sizeof(profile_t)};

int storage_load_all(profile_t *out, int max) {
    memset(out, 0, sizeof(profile_t) * max);
    FILE *f = fopen(PATH, "rb");
    if (!f) return 0;
    header_t h;
    if (fread(&h, sizeof h, 1, f) != 1 || memcmp(&h, &HEADER, sizeof h) != 0) {
        fclose(f);
        printf("perfis.bin é de outra versão do jogo: começando sem perfis.\n");
        remove(PATH);
        return 0;
    }
    size_t n = fread(out, sizeof(profile_t), max, f);
    fclose(f);
    int used = 0;
    for (size_t i = 0; i < n; i++) used += out[i].used;
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