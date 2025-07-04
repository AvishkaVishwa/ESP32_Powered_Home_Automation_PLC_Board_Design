#include <string.h>
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "wifi_config.h"

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_INFO
#endif

static const char *TAG = "WIFI_CONFIG";

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static EventGroupHandle_t wifi_event_group;
static int wifi_retry_num = 0;
static bool wifi_connected = false;

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < 10) {  // Increased retry attempts from 5 to 10
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(TAG, "Retry WiFi connection (%d/10)", wifi_retry_num);
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG, "WiFi connection failed after 10 attempts");
        }
        wifi_connected = false;
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        wifi_connected = true;
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "Station connected to AP, MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5]);
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "Station disconnected from AP, MAC: %02x:%02x:%02x:%02x:%02x:%02x", 
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3], event->mac[4], event->mac[5]);
    }
}

esp_err_t wifi_config_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    ESP_LOGI(TAG, "WiFi configuration system initialized");
    return ESP_OK;
}

esp_err_t wifi_config_load_credentials(wifi_credentials_t *credentials)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_CONFIG_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "No stored WiFi credentials found");
        credentials->configured = false;
        return ESP_ERR_NOT_FOUND;
    }

    size_t ssid_len = WIFI_SSID_MAX_LEN;
    size_t pass_len = WIFI_PASS_MAX_LEN;
    
    err = nvs_get_str(nvs_handle, "ssid", credentials->ssid, &ssid_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        credentials->configured = false;
        return err;
    }

    err = nvs_get_str(nvs_handle, "password", credentials->password, &pass_len);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        credentials->configured = false;
        return err;
    }

    nvs_close(nvs_handle);
    credentials->configured = true;
    ESP_LOGI(TAG, "Loaded WiFi credentials for SSID: %s", credentials->ssid);
    return ESP_OK;
}

esp_err_t wifi_config_save_credentials(const wifi_credentials_t *credentials)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(nvs_handle, "ssid", credentials->ssid);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_set_str(nvs_handle, "password", credentials->password);
    if (err != ESP_OK) {
        nvs_close(nvs_handle);
        return err;
    }

    err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    ESP_LOGI(TAG, "WiFi credentials saved for SSID: %s", credentials->ssid);
    return err;
}

esp_err_t wifi_config_start_ap_mode(void)
{
    esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(WIFI_AP_SSID),
            .channel = 1,
            .password = WIFI_AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    strcpy((char*)wifi_config.ap.ssid, WIFI_AP_SSID);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID: %s, Password: %s", WIFI_AP_SSID, WIFI_AP_PASS);
    ESP_LOGI(TAG, "Connect to AP and go to http://192.168.4.1 to configure WiFi");
    
    return ESP_OK;
}

esp_err_t wifi_config_connect_sta(const char* ssid, const char* password)
{
    ESP_LOGI(TAG, "Starting WiFi connection to: %s", ssid);
    
    // Reset retry counter for new connection attempt
    wifi_retry_num = 0;
    wifi_connected = false;
    
    // Clear any previous event bits
    xEventGroupClearBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    
    esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to WiFi SSID: %s", ssid);

    // Wait for connection or failure
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            pdMS_TO_TICKS(30000)); // 30 second timeout (increased from 10)

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Connected to WiFi successfully");
        return ESP_OK;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to WiFi");
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG, "WiFi connection timeout");
        return ESP_ERR_TIMEOUT;
    }
}

bool wifi_config_is_connected(void)
{
    return wifi_connected;
}

void wifi_config_reset(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(WIFI_CONFIG_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_erase_all(nvs_handle);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        ESP_LOGI(TAG, "WiFi credentials reset");
    }
}
