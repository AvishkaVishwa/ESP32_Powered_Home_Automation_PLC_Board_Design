#ifndef AUTO_BOARD_H
#define AUTO_BOARD_H

#include "driver/gpio.h"

// GPIO Pin Definitions for ESP32 WROOM-32D
// Optocoupler Inputs (12V-24V) - Active LOW (common for optocouplers)
#define INPUT_1_GPIO    GPIO_NUM_4
#define INPUT_2_GPIO    GPIO_NUM_5
#define INPUT_3_GPIO    GPIO_NUM_18
#define INPUT_4_GPIO    GPIO_NUM_19
#define INPUT_5_GPIO    GPIO_NUM_21

// SSR Outputs (230V AC Control) - Active HIGH
#define OUTPUT_1_GPIO   GPIO_NUM_12
#define OUTPUT_2_GPIO   GPIO_NUM_13
#define OUTPUT_3_GPIO   GPIO_NUM_14
//#define OUTPUT_4_GPIO   GPIO_NUM_27
#define OUTPUT_5_GPIO   GPIO_NUM_26

// Status LED
#define STATUS_LED_GPIO GPIO_NUM_2

// Configuration constants
#define NUM_INPUTS      5
#define NUM_OUTPUTS     4
#define DEBOUNCE_TIME_MS 50

// Input state structure
typedef struct {
    bool current_state;
    bool last_state;
    uint32_t last_change_time;
    bool debounced_state;
} input_state_t;

// Input event structure
typedef struct {
    uint8_t input_num;
    bool state;
} input_event_t;

// Function prototypes
void configure_gpio(void);
void set_output(uint8_t output_num, bool state);
void print_status(void);
void debounce_inputs(void);
void IRAM_ATTR gpio_isr_handler(void *arg);

// Task prototypes
void input_task(void *arg);
void output_control_task(void *arg);
void status_led_task(void *arg);
void timer_processing_task(void *arg);

#endif // AUTO_BOARD_H
