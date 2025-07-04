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

static const char *TAG = "AUTO_BOARD";

// Global variables
const gpio_num_t input_gpios[] = {
    INPUT_1_GPIO, INPUT_2_GPIO, INPUT_3_GPIO, INPUT_4_GPIO, INPUT_5_GPIO
};

const gpio_num_t output_gpios[] = {
    OUTPUT_1_GPIO, OUTPUT_2_GPIO, OUTPUT_3_GPIO, OUTPUT_4_GPIO, OUTPUT_5_GPIO
};

input_state_t input_states[NUM_INPUTS];
bool output_states[NUM_OUTPUTS];
QueueHandle_t input_event_queue;

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32 Automation Board");
    ESP_LOGI(TAG, "5 Optocoupler Inputs (12V-24V) + 5 SSR Outputs (230V AC)");
    ESP_LOGI(TAG, "Web Interface with WiFi Configuration and Timer Control");
    
    // Initialize WiFi configuration system
    ESP_LOGI(TAG, "Initializing WiFi configuration system...");
    wifi_config_init();
    
    // Try to load saved WiFi credentials
    wifi_credentials_t credentials;
    esp_err_t ret = wifi_config_load_credentials(&credentials);
    
    if (ret == ESP_OK && credentials.configured) {
        ESP_LOGI(TAG, "Found saved WiFi credentials, connecting to: %s", credentials.ssid);
        esp_err_t connect_ret = wifi_config_connect_sta(credentials.ssid, credentials.password);
        
        if (connect_ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to connect to saved WiFi, starting AP mode");
            wifi_config_start_ap_mode();
        }
    } else {
        ESP_LOGI(TAG, "No WiFi credentials found, starting AP mode for configuration");
        ESP_LOGI(TAG, "Connect to WiFi: ESP32-AutoBoard-Config (Password: automation123)");
        ESP_LOGI(TAG, "Then go to: http://192.168.4.1");
        wifi_config_start_ap_mode();
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
            ESP_LOGI(TAG, "Access the automation interface at: http://[ESP32_IP_ADDRESS]");
        } else {
            ESP_LOGI(TAG, "Access WiFi configuration at: http://192.168.4.1");
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