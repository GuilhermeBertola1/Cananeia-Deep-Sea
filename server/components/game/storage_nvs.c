// Perfis no ESP32: uma partição NVS própria ("perfis", 1 MB), um blob por perfil.
// Só entra na build do ESP-IDF; o PC usa server/host/storage_file.c.
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "storage.h"

static const char *TAG = "perfis";
static nvs_handle_t handle;
static int ready;

static int open_once(void) {
    if (ready) return 1;
    esp_err_t err = nvs_flash_init_partition("perfis");
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase_partition("perfis");
        err = nvs_flash_init_partition("perfis");
    }
    if (err == ESP_OK) err = nvs_open_from_partition("perfis", "perfis", NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "não abriu a partição de perfis: %s", esp_err_to_name(err));
        return 0;
    }
    ready = 1;
    return 1;
}

int storage_load_all(profile_t *out, int max) {
    memset(out, 0, sizeof(profile_t) * max);
    if (!open_once()) return 0;
    int count = 0;
    for (int i = 0; i < max; i++) {
        char key[12];
        snprintf(key, sizeof key, "p%03d", i);
        size_t size = sizeof(profile_t);
        if (nvs_get_blob(handle, key, &out[i], &size) != ESP_OK || size != sizeof(profile_t)) {
            memset(&out[i], 0, sizeof(profile_t));
            continue;
        }
        count += out[i].used;
    }
    ESP_LOGI(TAG, "%d perfis carregados", count);
    return count;
}

void storage_save(int index, const profile_t *p) {
    if (!open_once()) return;
    char key[12];
    snprintf(key, sizeof key, "p%03d", index);
    if (nvs_set_blob(handle, key, p, sizeof(profile_t)) == ESP_OK) nvs_commit(handle);
}
