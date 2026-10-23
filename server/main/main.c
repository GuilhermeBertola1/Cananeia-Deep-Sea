// Servidor do Cananeia Deep Sea no ESP32-S3.
// - Wi-Fi (ponto de acesso ou estação, ver menuconfig)
// - HTTP: serve o cliente web, que vai embutido no firmware (main/www.c)
// - WebSocket em /ws: mensagens binárias do jogo (docs/protocol.md)
// - Tarefa do jogo: game_tick() 10 vezes por segundo
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_random.h"
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

// --- WebSocket ----------------------------------------------------------------

static void ws_send(int slot, const uint8_t *data, size_t len) {
    if (slot_fd[slot] < 0) return;
    httpd_ws_frame_t frame = {
        .final = true,
        .type = HTTPD_WS_TYPE_BINARY,
        .payload = (uint8_t *)data,
        .len = len,
    };
    httpd_ws_send_frame_async(server, slot_fd[slot], &frame);
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
        if (slot >= 0) slot_fd[slot] = fd;
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

static esp_err_t file_handler(httpd_req_t *req) {
    const www_file_t *f = www_find(req->uri, strcspn(req->uri, "?#"));
    if (!f) return httpd_resp_send_404(req);
    httpd_resp_set_type(req, f->type);
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    return httpd_resp_send(req, (const char *)f->data, (ssize_t)f->size);
}

// --- inicialização ------------------------------------------------------------

static void start_http(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 12;
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.close_fn = on_close;
    cfg.stack_size = 8192;
    ESP_ERROR_CHECK(httpd_start(&server, &cfg));

    httpd_uri_t ws = {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true};
    httpd_uri_t files = {.uri = "/*", .method = HTTP_GET, .handler = file_handler};
    httpd_register_uri_handler(server, &ws);
    httpd_register_uri_handler(server, &files);
}

static void game_task(void *arg) {
    TickType_t last = xTaskGetTickCount();
    for (;;) {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000 / TICK_HZ));
        xSemaphoreTake(game_lock, portMAX_DELAY);
        game_tick();
        xSemaphoreGive(game_lock);
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
