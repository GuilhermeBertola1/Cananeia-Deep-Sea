// Robôs para o teste de carga: N jogadores falsos entram no servidor (ESP32 ou
// PC), navegam, atiram e vendem sozinhos. A cada minuto mostra quantos estão
// conectados, quantas vezes caíram, o maior buraco entre dois S_STATE e o
// /status do servidor. No fim diz se o teste passou.
//
// Uso: bots [ip] [porta] [robôs] [minutos]
//   bots                          -> 192.168.4.1 80 8 60 (ESP32 em ponto de acesso)
//   bots 127.0.0.1 8080 8 5       -> servidor do PC por 5 minutos
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "fish.h"
#include "protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET sock_t;
#define SEND_FLAGS 0
#define close_sock closesocket
#define BAD_SOCK INVALID_SOCKET
static uint64_t now_ms(void) { return GetTickCount64(); }
static void sleep_ms(int ms) { Sleep((DWORD)ms); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int sock_t;
#define SEND_FLAGS MSG_NOSIGNAL // no Linux, escrever num socket fechado não mata o programa
#define close_sock close
#define BAD_SOCK (-1)
static uint64_t now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}
static void sleep_ms(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000};
    nanosleep(&ts, NULL);
}
#endif

#define MAX_BOTS 16
#define IN_CAP 16384
#define PASSWORD "robo1234"
#define GAP_LIMIT_MS 2000 // mais que isso sem S_STATE conta como engasgo
#define RECONNECT_MS 3000

typedef struct {
    sock_t fd;
    int playing;              // recebeu S_WELCOME
    uint64_t last_state;      // quando chegou o último S_STATE
    uint64_t next_action;     // quando o robô decide de novo
    uint64_t reconnect_at;    // se caiu, quando tentar de novo
    int x, y;                 // posição (do S_STATE)
    uint8_t flags;
    uint8_t in[IN_CAP];
    size_t in_len;
} bot_t;

static bot_t bots[MAX_BOTS];
static int n_bots = 8;
static struct sockaddr_in server;
static const char *host = "192.168.4.1";
static int port = 80;

// Contadores do teste inteiro e do minuto atual.
static struct {
    unsigned drops, errors, joins;
    uint64_t states, states_min;
    uint64_t gap_max, gap_max_min; // maior buraco entre dois S_STATE (ms)
    unsigned stalls;               // buracos maiores que GAP_LIMIT_MS
} st;

static uint32_t rng = 12345;
static uint32_t rnd(void) {
    rng ^= rng << 13, rng ^= rng >> 17, rng ^= rng << 5;
    return rng;
}

// --- conexão ----------------------------------------------------------------

static sock_t tcp_connect(void) {
    sock_t fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == BAD_SOCK) return BAD_SOCK;
#ifdef _WIN32
    DWORD tmo = 3000;
#else
    struct timeval tmo = {3, 0};
#endif
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tmo, sizeof tmo);
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tmo, sizeof tmo);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof one);
    if (connect(fd, (struct sockaddr *)&server, sizeof server) != 0) {
        close_sock(fd);
        return BAD_SOCK;
    }
    return fd;
}

// Lê até achar o fim do cabeçalho HTTP. Devolve quantos bytes leu (0 = erro).
static int read_header(sock_t fd, char *buf, int cap) {
    int len = 0;
    while (len < cap - 1) {
        int n = recv(fd, buf + len, cap - 1 - len, 0);
        if (n <= 0) return 0;
        len += n;
        buf[len] = 0;
        if (strstr(buf, "\r\n\r\n")) return len;
    }
    return 0;
}

// Manda um frame binário mascarado (cliente sempre mascara).
static int ws_send(sock_t fd, const uint8_t *data, size_t len) {
    uint8_t frame[8 + 128];
    if (len > 125) return 0;
    uint8_t mask[4] = {(uint8_t)rnd(), (uint8_t)rnd(), (uint8_t)rnd(), (uint8_t)rnd()};
    frame[0] = 0x82;
    frame[1] = 0x80 | (uint8_t)len;
    memcpy(frame + 2, mask, 4);
    for (size_t i = 0; i < len; i++) frame[6 + i] = data[i] ^ mask[i % 4];
    return send(fd, (const char *)frame, (int)(6 + len), SEND_FLAGS) == (int)(6 + len);
}

static void bot_drop(int i, const char *why) {
    bot_t *b = &bots[i];
    if (b->fd != BAD_SOCK) close_sock(b->fd);
    if (b->playing) {
        st.drops++;
        printf("  robo%d caiu (%s)\n", i + 1, why);
    }
    b->fd = BAD_SOCK;
    b->playing = 0;
    b->in_len = 0;
    b->reconnect_at = now_ms() + RECONNECT_MS;
}

static void bot_connect(int i) {
    bot_t *b = &bots[i];
    b->fd = tcp_connect();
    if (b->fd == BAD_SOCK) { b->reconnect_at = now_ms() + RECONNECT_MS; return; }
    char req[256];
    int n = snprintf(req, sizeof req,
                     "GET /ws HTTP/1.1\r\nHost: %s\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                     "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n",
                     host);
    char resp[1024];
    if (send(b->fd, req, n, SEND_FLAGS) != n || !read_header(b->fd, resp, sizeof resp) || !strstr(resp, " 101 ")) {
        bot_drop(i, "aperto de mão");
        return;
    }
    // C_JOIN: u8 versão, u8 len, nome, u8 len, senha
    uint8_t join[64];
    char name[16];
    snprintf(name, sizeof name, "robo%d", i + 1);
    size_t nl = strlen(name), pl = strlen(PASSWORD), p = 0;
    join[p++] = C_JOIN;
    join[p++] = PROTOCOL_VERSION;
    join[p++] = (uint8_t)nl;
    memcpy(join + p, name, nl), p += nl;
    join[p++] = (uint8_t)pl;
    memcpy(join + p, PASSWORD, pl), p += pl;
    if (!ws_send(b->fd, join, p)) { bot_drop(i, "envio do C_JOIN"); return; }
    b->next_action = now_ms() + 500;
}

// --- o que o servidor manda ---------------------------------------------------

static void on_message(int i, const uint8_t *m, size_t len) {
    bot_t *b = &bots[i];
    if (len < 1) return;
    uint64_t now = now_ms();
    switch (m[0]) {
    case S_WELCOME:
        if (!b->playing) st.joins++;
        b->playing = 1;
        b->last_state = now;
        break;
    case S_ERROR:
        st.errors++;
        printf("  robo%d recebeu S_ERROR %d\n", i + 1, len > 1 ? m[1] : -1);
        break;
    case S_STATE:
        if (len < 9) return;
        if (b->last_state) {
            uint64_t gap = now - b->last_state;
            if (gap > st.gap_max) st.gap_max = gap;
            if (gap > st.gap_max_min) st.gap_max_min = gap;
            if (gap > GAP_LIMIT_MS) st.stalls++;
        }
        b->last_state = now;
        b->x = m[5] | m[6] << 8;
        b->y = m[7] | m[8] << 8;
        b->flags = len > 17 ? m[17] : 0; // u8 flags depois de tick, x, y, dinheiro, capacidade, speed, vel
        st.states++;
        st.states_min++;
        break;
    }
}

// Separa os frames que chegaram. Devolve 0 se a conexão tem que fechar.
static int bot_read(int i) {
    bot_t *b = &bots[i];
    int n = recv(b->fd, (char *)b->in + b->in_len, (int)(IN_CAP - b->in_len), 0);
    if (n <= 0) return 0;
    b->in_len += (size_t)n;
    for (;;) {
        if (b->in_len < 2) return 1;
        uint8_t op = b->in[0] & 0x0F;
        size_t len = b->in[1] & 0x7F, pos = 2;
        if (len == 126) {
            if (b->in_len < 4) return 1;
            len = (size_t)b->in[2] << 8 | b->in[3];
            pos = 4;
        } else if (len == 127) {
            return 0; // o jogo nunca manda frame tão grande
        }
        if (pos + len > IN_CAP) return 0;
        if (b->in_len < pos + len) return 1;
        if (op == 0x8) return 0;
        if (op == 0x2) on_message(i, b->in + pos, len);
        memmove(b->in, b->in + pos + len, b->in_len - pos - len);
        b->in_len -= pos + len;
    }
}

// --- o "cérebro" do robô --------------------------------------------------------

static void bot_think(int i) {
    bot_t *b = &bots[i];
    uint64_t now = now_ms();
    if (!b->playing || now < b->next_action) return;
    b->next_action = now + 1000 + rnd() % 3000;
    int ok = 1;
    uint32_t r = rnd() % 10;
    if (r < 6) {
        // Muda de direção (às vezes para, como quem está pescando).
        uint8_t m[3] = {C_INPUT, (uint8_t)(int8_t)(rnd() % 3 - 1), (uint8_t)(int8_t)(rnd() % 3 - 1)};
        ok = ws_send(b->fd, m, 3);
    } else if (r < 9 || !(b->flags & STATE_AT_DOCK)) {
        // Atira perto de si (o servidor confere arma, recarga e zona segura).
        int x = b->x + (int)(rnd() % 17) - 8, y = b->y + (int)(rnd() % 17) - 8;
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        uint8_t m[5] = {C_FIRE, (uint8_t)x, (uint8_t)(x >> 8), (uint8_t)y, (uint8_t)(y >> 8)};
        ok = ws_send(b->fd, m, 5);
    } else if (b->flags & STATE_AT_DOCK) {
        // No porto: vende tudo.
        uint8_t m[2] = {C_SELL, FISH_NONE};
        ok = ws_send(b->fd, m, 2);
    }
    if (!ok) bot_drop(i, "envio");
}

// --- /status do servidor ------------------------------------------------------

static void print_status(void) {
    sock_t fd = tcp_connect();
    if (fd == BAD_SOCK) { printf("  /status: sem resposta\n"); return; }
    char req[128], resp[1024];
    int n = snprintf(req, sizeof req, "GET /status HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    send(fd, req, n, SEND_FLAGS);
    int len = 0, got;
    while (len < (int)sizeof resp - 1 && (got = recv(fd, resp + len, (int)sizeof resp - 1 - len, 0)) > 0) len += got;
    resp[len] = 0;
    close_sock(fd);
    char *body = strstr(resp, "\r\n\r\n");
    printf("  /status: %s\n", body ? body + 4 : "resposta estranha");
}

// --- programa -------------------------------------------------------------------

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc > 1) host = argv[1];
    if (argc > 2) port = atoi(argv[2]);
    if (argc > 3) n_bots = atoi(argv[3]);
    int minutes = argc > 4 ? atoi(argv[4]) : 60;
    if (n_bots < 1) n_bots = 1;
    if (n_bots > MAX_BOTS) n_bots = MAX_BOTS;
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    server.sin_family = AF_INET;
    server.sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, host, &server.sin_addr) != 1) {
        fprintf(stderr, "IP inválido: %s\n", host);
        return 1;
    }
    rng ^= (uint32_t)time(NULL);
    printf("%d robôs em %s:%d por %d min (senha dos robôs: %s)\n", n_bots, host, port, minutes, PASSWORD);

    for (int i = 0; i < n_bots; i++) {
        bots[i].fd = BAD_SOCK;
        bot_connect(i);
        sleep_ms(300); // não chega todo mundo no mesmo milissegundo
    }

    uint64_t start = now_ms(), end = start + (uint64_t)minutes * 60000, next_report = start + 60000;
    while (now_ms() < end) {
        fd_set rd;
        FD_ZERO(&rd);
        sock_t maxfd = 0;
        for (int i = 0; i < n_bots; i++) {
            if (bots[i].fd == BAD_SOCK) continue;
            FD_SET(bots[i].fd, &rd);
            if (bots[i].fd > maxfd) maxfd = bots[i].fd;
        }
        struct timeval tv = {0, 50000};
        select((int)maxfd + 1, &rd, NULL, NULL, &tv);

        uint64_t now = now_ms();
        for (int i = 0; i < n_bots; i++) {
            bot_t *b = &bots[i];
            if (b->fd == BAD_SOCK) {
                if (now >= b->reconnect_at) bot_connect(i);
                continue;
            }
            if (FD_ISSET(b->fd, &rd) && !bot_read(i)) { bot_drop(i, "conexão fechada"); continue; }
            if (b->playing && now > b->last_state + 10000) { bot_drop(i, "10 s sem S_STATE"); continue; }
            bot_think(i);
        }

        if (now >= next_report) {
            next_report += 60000;
            int online = 0;
            for (int i = 0; i < n_bots; i++) online += bots[i].playing;
            printf("[%3d min] jogando %d/%d | estados/s %.1f | maior buraco %llu ms | quedas %u | erros %u\n",
                   (int)((now - start) / 60000), online, n_bots, st.states_min / 60.0,
                   (unsigned long long)st.gap_max_min, st.drops, st.errors);
            st.states_min = 0;
            st.gap_max_min = 0;
            print_status();
        }
    }

    int online = 0;
    for (int i = 0; i < n_bots; i++) online += bots[i].playing;
    printf("\nFim: %d/%d jogando, %u quedas, %u erros, %u engasgos (> %d ms), maior buraco %llu ms\n", online,
           n_bots, st.drops, st.errors, st.stalls, GAP_LIMIT_MS, (unsigned long long)st.gap_max);
    print_status();
    int passed = online == n_bots && st.drops == 0 && st.errors == 0 && st.stalls == 0;
    printf("%s\n", passed ? "PASSOU: o servidor aguentou." : "NÃO PASSOU: veja as quedas e o /status acima.");
    for (int i = 0; i < n_bots; i++)
        if (bots[i].fd != BAD_SOCK) close_sock(bots[i].fd);
    return passed ? 0 : 1;
}
