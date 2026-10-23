#pragma once
#include <stddef.h>
#include <stdint.h>

// Arquivos do cliente web embutidos no firmware (ver main/CMakeLists.txt).
typedef struct {
    const char *path;
    const char *type;
    const uint8_t *data;
    size_t size;
} www_file_t;

const www_file_t *www_find(const char *path, size_t path_len);
