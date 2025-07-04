#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "auto_board.h"
#include "auto_board_config.h"

// Fallback definition for IntelliSense
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_VERBOSE
#endif

static const char *TAG = "AUTO_BOARD";

// External variables (defined in main.c)
extern const gpio_num_t input_gpios[];
extern const gpio_num_t output_gpios[];
extern input_state_t input_states[];
extern bool output_states[];
extern QueueHandle_t input_event_queue;

void configure_gpio(void)
{
    ESP_LOGI(TAG, "Configuring GPIO pins");
    
    // Configure input pins with pull-up resistors
    gpio_config_t input_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    
    for (int i = 0; i < NUM_INPUTS; i++) {
        input_conf.pin_bit_mask |= (1ULL << input_gpios[i]);
    }
    
    gpio_config(&input_conf);
    
    // Configure output pins
    gpio_config_t output_conf = {
        .pin_bit_mask = 0,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        output_conf.pin_bit_mask |= (1ULL << output_gpios[i]);
    }
    
    gpio_config(&output_conf);
    
    // Configure status LED
    gpio_config_t led_conf = {
        .pin_bit_mask = (1ULL << STATUS_LED_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    gpio_config(&led_conf);
    
    // Install GPIO ISR service
    gpio_install_isr_service(0);
    
    // Add ISR handlers for input pins
    for (int i = 0; i < NUM_INPUTS; i++) {
        gpio_isr_handler_add(input_gpios[i], gpio_isr_handler, (void *)i);
    }
    
    ESP_LOGI(TAG, "GPIO configuration completed");
}

void gpio_isr_handler(void *arg)
{
    uint32_t input_num = (uint32_t)arg;
    input_event_t event = {
        .input_num = input_num,
        .state = gpio_get_level(input_gpios[input_num])
    };
    
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(input_event_queue, &event, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void debounce_inputs(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    for (int i = 0; i < NUM_INPUTS; i++) {
        if (input_states[i].current_state != input_states[i].last_state) {
            input_states[i].last_state = input_states[i].current_state;
            input_states[i].last_change_time = current_time;
        } else if ((current_time - input_states[i].last_change_time) >= DEBOUNCE_TIME_MS) {
            if (input_states[i].debounced_state != input_states[i].current_state) {
                input_states[i].debounced_state = input_states[i].current_state;
                ESP_LOGI(TAG, "Input %d debounced state: %s", 
                        i + 1, 
                        input_states[i].debounced_state ? "ACTIVE" : "INACTIVE");
            }
        }
    }
}

void set_output(uint8_t output_num, bool state)
{
    if (output_num < NUM_OUTPUTS) {
        gpio_set_level(output_gpios[output_num], state ? 1 : 0);
        output_states[output_num] = state;
    }
}

void print_status(void)
{
    ESP_LOGI(TAG, "=== AUTOMATION BOARD STATUS ===");
    
    // Print input states
    printf("INPUTS:  ");
    for (int i = 0; i < NUM_INPUTS; i++) {
        printf("IN%d:%s ", i + 1, input_states[i].debounced_state ? "ACT" : "INA");
    }
    printf("\n");
    
    // Print output states
    printf("OUTPUTS: ");
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        printf("OUT%d:%s ", i + 1, output_states[i] ? "ON" : "OFF");
    }
    printf("\n");
    
    ESP_LOGI(TAG, "===============================");
}
