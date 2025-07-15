#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "sdkconfig.h"

// Define default log level if not configured
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_INFO
#endif

// Define default FreeRTOS tick rate if not configured
#ifndef CONFIG_FREERTOS_HZ
#define CONFIG_FREERTOS_HZ 100
#endif
#include "auto_board.h"
#include "auto_board_config.h"
#include "web_server.h"
#include "wifi_config.h"
#include "mdns.h"

static const char *TAG = "AUTO_BOARD";

// Global variables
const gpio_num_t input_gpios[] = {
    INPUT_1_GPIO, INPUT_2_GPIO, INPUT_3_GPIO, INPUT_4_GPIO, INPUT_5_GPIO
};

const gpio_num_t output_gpios[] = {
    OUTPUT_1_GPIO, OUTPUT_2_GPIO, OUTPUT_3_GPIO, /* OUTPUT_4_GPIO, */ OUTPUT_5_GPIO
};

input_state_t input_states[NUM_INPUTS];
bool output_states[NUM_OUTPUTS];
QueueHandle_t input_event_queue;

static void wifi_reconnect_task(void *pvParameters)
{
    wifi_credentials_t *credentials = (wifi_credentials_t *)pvParameters;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(30000)); // Check every 30 seconds

        if (!wifi_config_is_connected()) {
            ESP_LOGI(TAG, "Periodically scanning for saved WiFi network...");
            wifi_config_scan_and_reconnect(credentials);
        } else {
            // If connected, we can delete this task
            ESP_LOGI(TAG, "WiFi is connected. Deleting reconnect task.");
            vTaskDelete(NULL);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 Automation Board");
    ESP_LOGI(TAG, "5 Optocoupler Inputs (12V-24V) + 5 SSR Outputs (230V AC)");
    ESP_LOGI(TAG, "Web Interface with WiFi Configuration and Timer Control");
    
    // Initialize WiFi configuration system
    ESP_LOGI(TAG, "Initializing WiFi configuration system...");
    wifi_config_init();

    // Initialize mDNS
    mdns_init();
    mdns_hostname_set("autoboard");
    mdns_instance_name_set("ESP32 Auto Board");
    
    // Try to load saved WiFi credentials
    wifi_credentials_t credentials;
    esp_err_t ret = wifi_config_load_credentials(&credentials);
    
    if (ret == ESP_OK && credentials.configured) {
        ESP_LOGI(TAG, "Found saved WiFi credentials, connecting to: %s", credentials.ssid);
        esp_err_t connect_ret = wifi_config_connect_sta(credentials.ssid, credentials.password);
        
        if (connect_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to connect to saved WiFi, starting AP mode");
            wifi_config_start_ap_mode();
            xTaskCreate(wifi_reconnect_task, "wifi_reconnect_task", 4096, &credentials, 5, NULL);
        }
    } else {
        ESP_LOGI(TAG, "No WiFi credentials found, starting AP mode for configuration");
        wifi_config_start_ap_mode();
        // Also start the reconnect task here, in case the user configures WiFi later
        xTaskCreate(wifi_reconnect_task, "wifi_reconnect_task", 4096, &credentials, 5, NULL);

        char ssid[WIFI_SSID_MAX_LEN];
        char password[WIFI_PASS_MAX_LEN];
        wifi_config_get_ap_credentials(ssid, password);
        
        esp_netif_ip_info_t ip_info;
        esp_err_t ip_ret = wifi_config_get_ap_ip(&ip_info);

        if (ip_ret == ESP_OK) {
            ESP_LOGI(TAG, "Connect to WiFi: %s (Password: %s)", ssid, password);
            ESP_LOGI(TAG, "Then go to: http://" IPSTR, IP2STR(&ip_info.ip));
        } else {
            ESP_LOGE(TAG, "Failed to get AP IP address");
        }
    }
    
    // Initialize GPIO
    configure_gpio();
    
    // Create input event queue
    input_event_queue = xQueueCreate(10, sizeof(input_event_t));
    if (input_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create input event queue");
        return;
    }
    
    // Initialize input states
    for (int i = 0; i < NUM_INPUTS; i++) {
        input_states[i].current_state = gpio_get_level(input_gpios[i]);
        input_states[i].last_state = input_states[i].current_state;
        input_states[i].debounced_state = input_states[i].current_state;
        input_states[i].last_change_time = esp_timer_get_time() / 1000;
    }
    
    // Initialize all outputs to OFF
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        set_output(i, false);
    }
    
    // Create tasks
    xTaskCreate(input_task, "input_task", 4096, NULL, 10, NULL);
    xTaskCreate(output_control_task, "output_control_task", 4096, NULL, 8, NULL);
    xTaskCreate(status_led_task, "status_led_task", 2048, NULL, 5, NULL);
    xTaskCreate(timer_processing_task, "timer_processing_task", 4096, NULL, 7, NULL);
    xTaskCreate(web_server_monitor_task, "web_monitor_task", 4096, NULL, 6, NULL);
    
    ESP_LOGI(TAG, "All tasks created successfully");
    
    // Start web server (with delay to allow WiFi setup)
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI(TAG, "Starting web server...");
    if (start_web_server() == ESP_OK) {
        ESP_LOGI(TAG, "Web server started successfully");
        if (wifi_config_is_connected()) {
            esp_netif_ip_info_t ip_info;
            esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
            if (netif) {
                esp_netif_get_ip_info(netif, &ip_info);
                ESP_LOGI(TAG, "Access the automation interface at: http://" IPSTR, IP2STR(&ip_info.ip));
                ESP_LOGI(TAG, "Or via mDNS at: http://autoboard.local");
            } else {
                ESP_LOGE(TAG, "Failed to get STA netif handle");
            }
        } else {
            esp_netif_ip_info_t ip_info;
            esp_err_t ip_ret = wifi_config_get_ap_ip(&ip_info);
            if (ip_ret == ESP_OK) {
                ESP_LOGI(TAG, "Access WiFi configuration at: http://" IPSTR, IP2STR(&ip_info.ip));
            } else {
                ESP_LOGE(TAG, "Failed to get AP IP address");
            }
        }
    } else {
        ESP_LOGE(TAG, "Failed to start web server");
    }
    
    // Main monitoring loop
    while (1) {
        print_status();
        vTaskDelay(pdMS_TO_TICKS(5000)); // Print status every 5 seconds
    }
}