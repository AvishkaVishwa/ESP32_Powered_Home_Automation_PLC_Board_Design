#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif.h"

// WiFi configuration constants
#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASS_MAX_LEN 64
#define WIFI_AP_SSID "ESP32-AutoBoard"
#define WIFI_AP_PASS "automation123"
#define WIFI_CONFIG_NAMESPACE "wifi_config"

// WiFi configuration structure
typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    char password[WIFI_PASS_MAX_LEN];
    bool configured;
} wifi_credentials_t;

// Function prototypes
esp_err_t wifi_config_init(void);
esp_err_t wifi_config_load_credentials(wifi_credentials_t *credentials);
esp_err_t wifi_config_save_credentials(const wifi_credentials_t *credentials);
esp_err_t wifi_config_start_ap_mode(void);
esp_err_t wifi_config_connect_sta(const char* ssid, const char* password);
bool wifi_config_is_connected(void);
void wifi_config_reset(void);
void wifi_config_get_ap_credentials(char* ssid, char* password);
esp_err_t wifi_config_get_ap_ip(esp_netif_ip_info_t *ip_info);
void wifi_config_scan_and_reconnect(const wifi_credentials_t *credentials);

#endif // WIFI_CONFIG_H
