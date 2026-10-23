#include "www.h"
#include <string.h>

#if HAS_WWW
// O ESP-IDF cria estes símbolos para cada arquivo de target_add_binary_data.
#define EMBED(name)                                                     \
    extern const uint8_t name##_start[] asm("_binary_" #name "_start"); \
    extern const uint8_t name##_end[] asm("_binary_" #name "_end");
EMBED(index_html)
EMBED(index_js)
EMBED(index_wasm)
EMBED(odin_js)

#define FILE(p, t, name) {p, t, name##_start, 0}
static www_file_t files[] = {
    FILE("/index.html", "text/html; charset=utf-8", index_html),
    FILE("/index.js", "text/javascript", index_js),
    FILE("/index.wasm", "application/wasm", index_wasm),
    FILE("/odin.js", "text/javascript", odin_js),
};
static const uint8_t *ends[] = {index_html_end, index_js_end, index_wasm_end, odin_js_end};
#else
static www_file_t files[] = {
    {"/index.html", "text/html; charset=utf-8",
     (const uint8_t *)"<h1>Cananeia Deep Sea</h1><p>Firmware sem o cliente. Rode client/build_web e compile de novo.</p>", 0},
};
#endif

const www_file_t *www_find(const char *path, size_t path_len) {
    if (path_len == 1 && path[0] == '/') path = "/index.html", path_len = strlen(path);
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) {
        www_file_t *f = &files[i];
        if (strlen(f->path) != path_len || memcmp(f->path, path, path_len) != 0) continue;
        if (!f->size) {
#if HAS_WWW
            f->size = (size_t)(ends[i] - f->data);
#else
            f->size = strlen((const char *)f->data);
#endif
        }
        return f;
    }
    return NULL;
}
