#include "wifi.h"
#include <string.h>
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#if CONFIG_CANANEIA_NET_QEMU
#include "esp_eth.h"
#endif
#include "sdkconfig.h"

static const char *TAG = "wifi";

#if CONFIG_CANANEIA_WIFI_STA
static void on_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && (id == WIFI_EVENT_STA_START || id == WIFI_EVENT_STA_DISCONNECTED)) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = data;
        ESP_LOGI(TAG, "Conectado! Abra http://" IPSTR " no navegador", IP2STR(&ev->ip_info.ip));
    }
}
#endif

#if CONFIG_CANANEIA_NET_QEMU
// Rede virtual do QEMU (open_eth). Com "-nic user,hostfwd=tcp::8080-:80"
// o jogo abre em http://localhost:8080 no PC.
void wifi_start(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_config_t netif_cfg = ESP_NETIF_DEFAULT_ETH();
    esp_netif_t *netif = esp_netif_new(&netif_cfg);
    eth_mac_config_t mac_cfg = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_cfg = ETH_PHY_DEFAULT_CONFIG();
    phy_cfg.autonego_timeout_ms = 100;
    esp_eth_mac_t *mac = esp_eth_mac_new_openeth(&mac_cfg);
    esp_eth_phy_t *phy = esp_eth_phy_new_generic(&phy_cfg);
    esp_eth_config_t eth_cfg = ETH_DEFAULT_CONFIG(mac, phy);
    esp_eth_handle_t eth;
    ESP_ERROR_CHECK(esp_eth_driver_install(&eth_cfg, &eth));
    ESP_ERROR_CHECK(esp_netif_attach(netif, esp_eth_new_netif_glue(eth)));
    ESP_ERROR_CHECK(esp_eth_start(eth));
    ESP_LOGI(TAG, "Rede do QEMU ligada");
}
#else
void wifi_start(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init));
    wifi_config_t cfg = {0};

#if CONFIG_CANANEIA_WIFI_STA
    esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_event, NULL));
    strlcpy((char *)cfg.sta.ssid, CONFIG_CANANEIA_WIFI_SSID, sizeof cfg.sta.ssid);
    strlcpy((char *)cfg.sta.password, CONFIG_CANANEIA_WIFI_PASSWORD, sizeof cfg.sta.password);
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_LOGI(TAG, "Entrando na rede \"%s\"...", CONFIG_CANANEIA_WIFI_SSID);
#else
    esp_netif_create_default_wifi_ap();
    strlcpy((char *)cfg.ap.ssid, CONFIG_CANANEIA_WIFI_SSID, sizeof cfg.ap.ssid);
    strlcpy((char *)cfg.ap.password, CONFIG_CANANEIA_WIFI_PASSWORD, sizeof cfg.ap.password);
    cfg.ap.ssid_len = strlen(CONFIG_CANANEIA_WIFI_SSID);
    cfg.ap.channel = 6;
    cfg.ap.max_connection = 8;
    cfg.ap.authmode = strlen(CONFIG_CANANEIA_WIFI_PASSWORD) >= 8 ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &cfg));
    ESP_LOGI(TAG, "Rede \"%s\" criada. Conecte nela e abra http://192.168.4.1", CONFIG_CANANEIA_WIFI_SSID);
#endif
    ESP_ERROR_CHECK(esp_wifi_start());
}
#endif
