#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "auto_board.h"
#include "auto_board_config.h"
#include "web_server.h"

// Define pdMS_TO_TICKS if not defined (for ESP-IDF compatibility)
#ifndef pdMS_TO_TICKS
#define pdMS_TO_TICKS(x) ((TickType_t)((x) / portTICK_PERIOD_MS))
#endif

// Fallback definitions for IntelliSense
#ifndef CONFIG_FREERTOS_HZ
#define CONFIG_FREERTOS_HZ 1000
#endif

#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_VERBOSE
#endif

static const char *TAG = "AUTO_BOARD_TASKS";

// External variables (defined in main.c)
extern const gpio_num_t input_gpios[];
extern const gpio_num_t output_gpios[];
extern input_state_t input_states[];
extern bool output_states[];
extern QueueHandle_t input_event_queue;

void input_task(void *arg)
{
    input_event_t event;
    
    ESP_LOGI(TAG, "Input monitoring task started");
    
    while (1) {
        if (xQueueReceive(input_event_queue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            // Update input state
            input_states[event.input_num].current_state = event.state;
            input_states[event.input_num].last_change_time = esp_timer_get_time() / 1000;
            
            ESP_LOGD(TAG, "Input %d changed to %s", 
                    event.input_num + 1, 
                    event.state ? "HIGH" : "LOW");
        }
        
        // Perform debouncing
        debounce_inputs();
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void output_control_task(void *arg)
{
    ESP_LOGI(TAG, "Output control task started");
    
    while (1) {
        // Simple logic: Each input controls corresponding output
        // You can modify this logic based on your automation requirements
        for (int i = 0; i < NUM_INPUTS && i < NUM_OUTPUTS; i++) {
            // Optocouplers are typically active LOW, so invert the logic
            bool desired_output_state = !input_states[i].debounced_state;
            
            if (output_states[i] != desired_output_state) {
                set_output(i, desired_output_state);
                ESP_LOGI(TAG, "Output %d set to %s (Input %d triggered)", 
                        i + 1, 
                        desired_output_state ? "ON" : "OFF",
                        i + 1);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void status_led_task(void *arg)
{
    ESP_LOGI(TAG, "Status LED task started");
    
    while (1) {
        // Blink pattern indicates system is running
        gpio_set_level(STATUS_LED_GPIO, 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        gpio_set_level(STATUS_LED_GPIO, 0);
        vTaskDelay(pdMS_TO_TICKS(900));
    }
}

void timer_processing_task(void *arg)
{
    ESP_LOGI(TAG, "Timer processing task started");
    
    while (1) {
        // Process all active timers
        process_timers();
        
        // Check every 10 seconds
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
