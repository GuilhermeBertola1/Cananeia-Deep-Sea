// Servidor de teste para o PC: o mesmo núcleo do ESP32 (components/game),
// com um servidor HTTP + WebSocket mínimo no lugar do esp_http_server.
// Serve o cliente web compilado e o jogo em ws://localhost:8080/ws.
//
// Uso: cananeia-host [pasta-do-cliente] [porta]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "game.h"
#include "world.h"

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET sock_t;
#define close_sock closesocket
#define BAD_SOCK INVALID_SOCKET
static uint64_t now_ms(void) { return GetTickCount64(); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define close_sock close
#define BAD_SOCK (-1)
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
#endif

#define MAX_CLIENTS 16
#define IN_CAP 8192

typedef struct {
    sock_t fd;
    int is_ws;
    int slot; // slot do jogo, -1 se não tem
    uint8_t in[IN_CAP];
    size_t in_len;
} client_t;

static client_t clients[MAX_CLIENTS];
static int slot_client[MAX_PLAYERS];
static const char *www = "../client/build/web";

// --- SHA-1 e base64, só para o aperto de mão do WebSocket --------------------

static uint32_t rol(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

static void sha1(const uint8_t *data, size_t len, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    size_t total = ((len + 8) / 64 + 1) * 64;
    uint8_t *msg = calloc(total, 1);
    memcpy(msg, data, len);
    msg[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) msg[total - 1 - i] = (uint8_t)(bits >> (8 * i));
    for (size_t off = 0; off < total; off += 64) {
        uint32_t w[80];
        for (int i = 0; i < 16; i++)
            w[i] = (uint32_t)msg[off + 4 * i] << 24 | msg[off + 4 * i + 1] << 16 | msg[off + 4 * i + 2] << 8 |
                   msg[off + 4 * i + 3];
        for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        for (int i = 0; i < 80; i++) {
            uint32_t f, k;
            if (i < 20) f = (b & c) | (~b & d), k = 0x5A827999;
            else if (i < 40) f = b ^ c ^ d, k = 0x6ED9EBA1;
            else if (i < 60) f = (b & c) | (b & d) | (c & d), k = 0x8F1BBCDC;
            else f = b ^ c ^ d, k = 0xCA62C1D6;
            uint32_t t = rol(a, 5) + f + e + k + w[i];
            e = d, d = c, c = rol(b, 30), b = a, a = t;
        }
        h[0] += a, h[1] += b, h[2] += c, h[3] += d, h[4] += e;
    }
    free(msg);
    for (int i = 0; i < 20; i++) out[i] = (uint8_t)(h[i / 4] >> (24 - 8 * (i % 4)));
}

static void base64(const uint8_t *in, size_t len, char *out) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t o = 0;
    for (size_t i = 0; i < len; i += 3) {
        uint32_t v = in[i] << 16 | (i + 1 < len ? in[i + 1] << 8 : 0) | (i + 2 < len ? in[i + 2] : 0);
        out[o++] = T[v >> 18 & 63];
        out[o++] = T[v >> 12 & 63];
        out[o++] = i + 1 < len ? T[v >> 6 & 63] : '=';
        out[o++] = i + 2 < len ? T[v & 63] : '=';
    }
    out[o] = 0;
}

// --- envio ------------------------------------------------------------------

static void send_raw(sock_t fd, const void *data, size_t len) {
    const char *p = data;
    while (len > 0) {
        int n = send(fd, p, (int)len, 0);
        if (n <= 0) return;
        p += n;
        len -= (size_t)n;
    }
}

static void ws_send(sock_t fd, uint8_t opcode, const uint8_t *data, size_t len) {
    uint8_t head[4] = {0x80 | opcode};
    size_t head_len = 2;
    if (len < 126) {
        head[1] = (uint8_t)len;
    } else {
        head[1] = 126;
        head[2] = (uint8_t)(len >> 8);
        head[3] = (uint8_t)len;
        head_len = 4;
    }
    send_raw(fd, head, head_len);
    send_raw(fd, data, len);
}

static void game_send(int slot, const uint8_t *data, size_t len) {
    int c = slot_client[slot];
    if (c >= 0) ws_send(clients[c].fd, 0x2, data, len);
}

// --- HTTP -------------------------------------------------------------------

static const char *content_type(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return "application/octet-stream";
    if (!strcmp(dot, ".html")) return "text/html; charset=utf-8";
    if (!strcmp(dot, ".js")) return "text/javascript";
    if (!strcmp(dot, ".wasm")) return "application/wasm";
    if (!strcmp(dot, ".css")) return "text/css";
    return "application/octet-stream";
}

static void http_reply(sock_t fd, const char *status, const char *type, const void *body, size_t len) {
    char head[256];
    int n = snprintf(head, sizeof head,
                     "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nCache-Control: no-cache\r\n"
                     "Connection: close\r\n\r\n",
                     status, type, len);
    send_raw(fd, head, (size_t)n);
    send_raw(fd, body, len);
}

static void serve_file(sock_t fd, const char *url) {
    char path[512];
    if (!strcmp(url, "/")) url = "/index.html";
    if (strstr(url, "..")) { http_reply(fd, "403 Forbidden", "text/plain", "403", 3); return; }
    snprintf(path, sizeof path, "%s%s", www, url);
    FILE *f = fopen(path, "rb");
    if (!f) { http_reply(fd, "404 Not Found", "text/plain", "404", 3); return; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *body = malloc((size_t)size);
    size_t got = fread(body, 1, (size_t)size, f);
    fclose(f);
    http_reply(fd, "200 OK", content_type(path), body, got);
    free(body);
}

static void drop(int c) {
    if (clients[c].slot >= 0) {
        slot_client[clients[c].slot] = -1;
        game_disconnect(clients[c].slot);
    }
    close_sock(clients[c].fd);
    clients[c].fd = BAD_SOCK;
}

// Lê o cabeçalho HTTP. Devolve 1 se a conexão continua aberta (virou WebSocket).
static int handle_http(int c) {
    client_t *cl = &clients[c];
    cl->in[cl->in_len < IN_CAP ? cl->in_len : IN_CAP - 1] = 0;
    char *end = strstr((char *)cl->in, "\r\n\r\n");
    if (!end) return cl->in_len < IN_CAP - 1;

    char method[8] = {0}, url[256] = {0};
    sscanf((char *)cl->in, "%7s %255s", method, url);
    char *key = strstr((char *)cl->in, "Sec-WebSocket-Key:");
    if (!key) key = strstr((char *)cl->in, "sec-websocket-key:");

    if (strcmp(url, "/ws") != 0 || !key) {
        serve_file(cl->fd, url);
        return 0;
    }

    key += strlen("Sec-WebSocket-Key:");
    while (*key == ' ') key++;
    char accept_src[128];
    size_t klen = strcspn(key, "\r\n");
    snprintf(accept_src, sizeof accept_src, "%.*s258EAFA5-E914-47DA-95CA-C5AB0DC85B11", (int)klen, key);
    uint8_t digest[20];
    char accept[32];
    sha1((uint8_t *)accept_src, strlen(accept_src), digest);
    base64(digest, 20, accept);
    char resp[256];
    int n = snprintf(resp, sizeof resp,
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Accept: %s\r\n\r\n",
                     accept);
    send_raw(cl->fd, resp, (size_t)n);

    cl->is_ws = 1;
    cl->in_len = 0;
    cl->slot = game_connect();
    if (cl->slot < 0) {
        const uint8_t full[] = {0x82, 3}; // S_ERROR, ERR_FULL
        ws_send(cl->fd, 0x2, full, sizeof full);
        return 0;
    }
    slot_client[cl->slot] = c;
    printf("conexão %d entrou no slot %d\n", c, cl->slot);
    return 1;
}

// Processa os frames completos do buffer. Devolve 0 para fechar.
static int handle_ws(int c) {
    client_t *cl = &clients[c];
    for (;;) {
        if (cl->in_len < 2) return 1;
        uint8_t *b = cl->in;
        uint8_t opcode = b[0] & 0x0F;
        uint64_t len = b[1] & 0x7F;
        size_t pos = 2;
        if (len == 126) {
            if (cl->in_len < 4) return 1;
            len = (uint64_t)b[2] << 8 | b[3];
            pos = 4;
        } else if (len == 127) {
            return 0; // mensagens do jogo são pequenas
        }
        if (!(b[1] & 0x80)) return 0; // cliente sempre mascara
        if (cl->in_len < pos + 4 + len) return cl->in_len < IN_CAP;
        uint8_t *mask = b + pos, *payload = b + pos + 4;
        for (uint64_t i = 0; i < len; i++) payload[i] ^= mask[i % 4];

        if (opcode == 0x8) return 0;
        if (opcode == 0x9) ws_send(cl->fd, 0xA, payload, (size_t)len);
        if (opcode == 0x2 && cl->slot >= 0) game_receive(cl->slot, payload, (size_t)len);

        size_t used = pos + 4 + (size_t)len;
        memmove(cl->in, cl->in + used, cl->in_len - used);
        cl->in_len -= used;
    }
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) www = argv[1];
    int port = argc > 2 ? atoi(argv[2]) : 8080;
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = BAD_SOCK;
    for (int i = 0; i < MAX_PLAYERS; i++) slot_client[i] = -1;

    uint8_t *tiles = malloc(MAP_W * MAP_H);
    game_init(tiles, 1337, (uint32_t)time(NULL), game_send);

    sock_t srv = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char *)&yes, sizeof yes);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(srv, 8) != 0) {
        fprintf(stderr, "não consegui abrir a porta %d\n", port);
        return 1;
    }
    printf("Cananeia Deep Sea: abra http://localhost:%d (cliente em %s)\n", port, www);
    fflush(stdout);

    uint64_t next_tick = now_ms();
    for (;;) {
        fd_set rd;
        FD_ZERO(&rd);
        FD_SET(srv, &rd);
        sock_t maxfd = srv;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd == BAD_SOCK) continue;
            FD_SET(clients[i].fd, &rd);
            if (clients[i].fd > maxfd) maxfd = clients[i].fd;
        }
        uint64_t now = now_ms();
        uint64_t wait = next_tick > now ? next_tick - now : 0;
        struct timeval tv = {(long)(wait / 1000), (long)(wait % 1000) * 1000};
        select((int)maxfd + 1, &rd, NULL, NULL, &tv);

        if (FD_ISSET(srv, &rd)) {
            sock_t fd = accept(srv, NULL, NULL);
            int c = 0;
            while (c < MAX_CLIENTS && clients[c].fd != BAD_SOCK) c++;
            if (c == MAX_CLIENTS) {
                close_sock(fd);
            } else {
                clients[c] = (client_t){.fd = fd, .slot = -1};
            }
        }
        for (int c = 0; c < MAX_CLIENTS; c++) {
            client_t *cl = &clients[c];
            if (cl->fd == BAD_SOCK || !FD_ISSET(cl->fd, &rd)) continue;
            int n = recv(cl->fd, (char *)cl->in + cl->in_len, (int)(IN_CAP - 1 - cl->in_len), 0);
            if (n <= 0) {
                drop(c);
                continue;
            }
            cl->in_len += (size_t)n;
            int keep = cl->is_ws ? handle_ws(c) : handle_http(c);
            if (!keep) drop(c);
        }

        if (now_ms() >= next_tick) {
            game_tick();
            next_tick += 1000 / TICK_HZ;
            if (now_ms() > next_tick + 1000) next_tick = now_ms(); // ficou para trás demais
        }
    }
}
