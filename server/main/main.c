// Servidor do Cananeia Deep Sea no ESP32-S3.
// - Wi-Fi (ponto de acesso ou estação, ver menuconfig)
// - HTTP: serve o cliente web, que vai embutido no firmware (main/www.c)
// - WebSocket em /ws: mensagens binárias do jogo (docs/protocol.md)
// - Tarefa do jogo: game_tick() 10 vezes por segundo
// - /status: saúde do servidor em JSON (memória, tick mais lento...), também
//   escrita no monitor serial a cada minuto
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "game.h"
#include "world.h"
#include "wifi.h"
#include "www.h"

static const char *TAG = "cananeia";

static httpd_handle_t server;
static SemaphoreHandle_t game_lock; // o jogo não é thread-safe: uma chamada por vez
static int slot_fd[MAX_PLAYERS];
static bool slot_closing[MAX_PLAYERS]; // envio falhou, a conexão está sendo fechada

// Saúde do servidor, para o teste de carga. Só mexe quem segura o game_lock.
static struct {
    int64_t tick_sum_us;  // soma da duração dos ticks no minuto atual (para a média)
    uint32_t ticks;       // ticks no minuto atual
    int32_t tick_max_us;  // tick mais demorado desde que ligou
    uint32_t late_ticks;  // vezes que o jogo ficou para trás (tick não coube em 100 ms)
    uint32_t send_fail;   // envios que falharam (jogador sumiu ou a rede travou)
    uint32_t joins;       // conexões que entraram no jogo desde que ligou
} stats;

// --- WebSocket ----------------------------------------------------------------

static void ws_send(int slot, const uint8_t *data, size_t len) {
    if (slot_fd[slot] < 0 || slot_closing[slot]) return;
    httpd_ws_frame_t frame = {
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = (uint8_t *)data,
        .len = len,
    };
    // Se o envio falha (celular saiu do Wi-Fi, rede entupida), fecha a conexão
    // em vez de tentar de novo todo tick: o on_close salva o perfil e libera o slot.
    if (httpd_ws_send_frame_async(server, slot_fd[slot], &frame) != ESP_OK) {
        stats.send_fail++;
        slot_closing[slot] = true;
        httpd_sess_trigger_close(server, slot_fd[slot]);
    }
}

static int slot_of(int fd) {
    for (int i = 0; i < MAX_PLAYERS; i++)
        if (slot_fd[i] == fd) return i;
    return -1;
}

static esp_err_t ws_handler(httpd_req_t *req) {
    int fd = httpd_req_to_sockfd(req);
    uint8_t buf[64];
    httpd_ws_frame_t frame = {0};
    esp_err_t err = httpd_ws_recv_frame(req, &frame, 0); // primeiro só o tamanho
    if (err != ESP_OK || frame.len > sizeof buf) return ESP_FAIL;
    frame.payload = buf;
    err = httpd_ws_recv_frame(req, &frame, sizeof buf);
    if (err != ESP_OK) return err;
    if (frame.type != HTTPD_WS_TYPE_BINARY) return ESP_OK;

    xSemaphoreTake(game_lock, portMAX_DELAY);
    int slot = slot_of(fd);
    if (slot < 0) {
        // Primeira mensagem desta conexão: reserva um lugar no jogo.
        slot = game_connect();
        if (slot >= 0) {
            slot_fd[slot] = fd;
            slot_closing[slot] = false;
            stats.joins++;
        }
        ESP_LOGI(TAG, "conexão %d -> slot %d", fd, slot);
    }
    if (slot >= 0) game_receive(slot, buf, frame.len);
    xSemaphoreGive(game_lock);

    if (slot < 0) {
        uint8_t full[] = {0x82, 3}; // S_ERROR, ERR_FULL
        httpd_ws_frame_t reply = {.final = true, .type = HTTPD_WS_TYPE_BINARY, .payload = full, .len = 2};
        httpd_ws_send_frame(req, &reply);
    }
    return ESP_OK;
}

static void on_close(httpd_handle_t hd, int fd) {
    xSemaphoreTake(game_lock, portMAX_DELAY);
    int slot = slot_of(fd);
    if (slot >= 0) {
        slot_fd[slot] = -1;
        game_disconnect(slot);
        ESP_LOGI(TAG, "slot %d saiu", slot);
    }
    xSemaphoreGive(game_lock);
    close(fd);
}

// --- arquivos do cliente ------------------------------------------------------

// Depois de responder uma página, fecha a conexão. Os sockets do ESP são
// poucos (13) e devem ficar com os jogadores, não com navegadores parados.
static void close_after_reply(httpd_req_t *req) {
    httpd_sess_trigger_close(req->handle, httpd_req_to_sockfd(req));
}

static esp_err_t file_handler(httpd_req_t *req) {
    const www_file_t *f = www_find(req->uri, strcspn(req->uri, "?#"));
    if (!f) {
        httpd_resp_send_404(req);
        close_after_reply(req);
        return ESP_OK;
    }
    httpd_resp_set_type(req, f->type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Connection", "close");
    esp_err_t err = httpd_resp_send(req, (const char *)f->data, (ssize_t)f->size);
    close_after_reply(req);
    return err;
}

// --- saúde do servidor --------------------------------------------------------

// Escreve a saúde do servidor em JSON. Também zera a média do minuto se `new_minute`.
static void format_status(char *buf, size_t cap, bool new_minute) {
    xSemaphoreTake(game_lock, portMAX_DELAY);
    int players = game_online();
    int32_t avg = stats.ticks ? (int32_t)(stats.tick_sum_us / stats.ticks) : 0;
    snprintf(buf, cap,
             "{\"version\":\"%s\",\"uptime_s\":%" PRId64 ",\"players\":%d,\"joins\":%" PRIu32
             ",\"heap_free\":%u,\"heap_min\":%u,\"ram_free\":%u,\"psram_free\":%u"
             ",\"tick_avg_us\":%" PRId32 ",\"tick_max_us\":%" PRId32 ",\"late_ticks\":%" PRIu32
             ",\"send_fail\":%" PRIu32 "}",
             esp_app_get_description()->version, esp_timer_get_time() / 1000000, players, stats.joins,
             (unsigned)esp_get_free_heap_size(), (unsigned)esp_get_minimum_free_heap_size(),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM), avg, stats.tick_max_us, stats.late_ticks,
             stats.send_fail);
    if (new_minute) stats.tick_sum_us = 0, stats.ticks = 0;
    xSemaphoreGive(game_lock);
}

static esp_err_t status_handler(httpd_req_t *req) {
    char buf[400];
    format_status(buf, sizeof buf, false);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, buf);
    close_after_reply(req);
    return ESP_OK;
}

// --- inicialização ------------------------------------------------------------

static void start_http(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    // 13 = CONFIG_LWIP_MAX_SOCKETS (16) menos os 3 que o próprio servidor usa.
    // Sem "LRU purge": ele derrubava o jogador mais parado quando alguém abria
    // a página. Em troca, as páginas fecham a conexão logo depois de responder.
    cfg.max_open_sockets = 13;
    cfg.lru_purge_enable = false;
    // Keep-alive do TCP: um celular que sumiu do Wi-Fi sem avisar é descoberto
    // em uns 20 s, e o slot dele fica livre.
    cfg.keep_alive_enable = true;
    cfg.keep_alive_idle = 10;
    cfg.keep_alive_interval = 3;
    cfg.keep_alive_count = 3;
    // Um envio pode esperar no máximo 1 s. Mais que isso travaria o jogo de todos.
    cfg.send_wait_timeout = 1;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.close_fn = on_close;
    cfg.stack_size = 8192;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    httpd_uri_t ws = {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true};
    httpd_uri_t status = {.uri = "/status", .method = HTTP_GET, .handler = status_handler};
    httpd_uri_t files = {.uri = "/*", .method = HTTP_GET, .handler = file_handler};
    httpd_register_uri_handler(server, &ws);
    httpd_register_uri_handler(server, &status); // antes do "/*", que pega tudo
    httpd_register_uri_handler(server, &files);
}

static void game_task(void *arg) {
    TickType_t last = xTaskGetTickCount();
    int64_t next_log = esp_timer_get_time() + 60 * 1000000LL;
    for (;;) {
        // xTaskDelayUntil devolve pdFALSE quando não precisou esperar: o tick
        // anterior (ou alguém segurando o game_lock) passou do tempo.
        bool late = xTaskDelayUntil(&last, pdMS_TO_TICKS(1000 / TICK_HZ)) == pdFALSE;
        xSemaphoreTake(game_lock, portMAX_DELAY);
        int64_t t0 = esp_timer_get_time();
        game_tick();
        int32_t us = (int32_t)(esp_timer_get_time() - t0);
        stats.tick_sum_us += us;
        stats.ticks++;
        if (us > stats.tick_max_us) stats.tick_max_us = us;
        if (late) stats.late_ticks++;
        xSemaphoreGive(game_lock);

        if (esp_timer_get_time() >= next_log) {
            next_log += 60 * 1000000LL;
            char buf[400];
            format_status(buf, sizeof buf, true);
            ESP_LOGI(TAG, "saúde: %s", buf);
        }
    }
}

void app_main(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    for (int i = 0; i < MAX_PLAYERS; i++) slot_fd[i] = -1;
    game_lock = xSemaphoreCreateMutex();

    // O mapa inteiro (256 KB) fica na PSRAM.
    uint8_t *tiles = heap_caps_malloc(MAP_W * MAP_H, MALLOC_CAP_SPIRAM);
    if (!tiles) tiles = malloc(MAP_W * MAP_H);
    if (!tiles) {
        ESP_LOGE(TAG, "sem memória para o mapa: a PSRAM está ligada?");
        return;
    }
    game_init(tiles, CONFIG_CANANEIA_MAP_SEED, esp_random(), ws_send);
    ESP_LOGI(TAG, "mapa gerado (semente %d)", CONFIG_CANANEIA_MAP_SEED);

    wifi_start();
    start_http();
    xTaskCreatePinnedToCore(game_task, "jogo", 8192, NULL, 5, NULL, 1);
}
