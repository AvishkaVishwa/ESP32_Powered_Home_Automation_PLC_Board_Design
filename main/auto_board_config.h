#ifndef AUTO_BOARD_CONFIG_H
#define AUTO_BOARD_CONFIG_H

// Hardware Configuration
#define BOARD_VERSION "1.0"
#define BOARD_NAME "ESP32 Automation Board"

// Input Configuration
#define INPUT_ACTIVE_LOW        1    // 1 = Active LOW, 0 = Active HIGH
#define INPUT_DEBOUNCE_MS       100   // Debounce time in milliseconds
#define INPUT_PULLUP_ENABLE     1    // Enable internal pull-up resistors

// Output Configuration  
#define OUTPUT_ACTIVE_HIGH      1    // 1 = Active HIGH, 0 = Active LOW
#define OUTPUT_DEFAULT_STATE    0    // Default output state on startup (0 = OFF)

// Safety Configuration
#define ENABLE_WATCHDOG         1    // Enable watchdog timer
#define WATCHDOG_TIMEOUT_S      5    // Watchdog timeout in seconds
#define ENABLE_BROWNOUT_DET     1    // Enable brownout detection

// Task Configuration
#define INPUT_TASK_PRIORITY     10   // Input task priority (higher = more priority)
#define OUTPUT_TASK_PRIORITY    8    // Output control task priority
#define STATUS_TASK_PRIORITY    5    // Status LED task priority

#define INPUT_TASK_STACK_SIZE   4096 // Input task stack size
#define OUTPUT_TASK_STACK_SIZE  4096 // Output task stack size
#define STATUS_TASK_STACK_SIZE  2048 // Status task stack size

// Logging Configuration
#define LOG_LEVEL_DEFAULT       3    // 0=None, 1=Error, 2=Warn, 3=Info, 4=Debug, 5=Verbose
#define ENABLE_STATUS_PRINT     1    // Enable periodic status printing
#define STATUS_PRINT_INTERVAL_S 5    // Status print interval in seconds

// Communication Configuration
#define SERIAL_BAUD_RATE        115200
#define ENABLE_REMOTE_CONTROL   0    // Enable WiFi/Bluetooth remote control (future feature)

// Control Logic Configuration
#define CONTROL_MODE_DIRECT     1    // Direct input-to-output mapping
#define CONTROL_MODE_CUSTOM     0    // Custom control logic

// Input-Output Mapping (when using direct mode)
// Map each input to corresponding output (1-based indexing)
#define INPUT_1_CONTROLS_OUTPUT 1
#define INPUT_2_CONTROLS_OUTPUT 2
#define INPUT_3_CONTROLS_OUTPUT 3
#define INPUT_4_CONTROLS_OUTPUT 4
#define INPUT_5_CONTROLS_OUTPUT 5

// Advanced Features
#define ENABLE_INPUT_INTERRUPTS 1    // Use GPIO interrupts for inputs
#define ENABLE_OUTPUT_FEEDBACK  0    // Monitor output states (future feature)
#define ENABLE_OVERCURRENT_DET  0    // Enable overcurrent detection (future feature)

// Timing Configuration
#define MAIN_LOOP_DELAY_MS      100  // Main task loop delay
#define LED_BLINK_ON_MS         100  // Status LED on time
#define LED_BLINK_OFF_MS        900  // Status LED off time

#endif // AUTO_BOARD_CONFIG_H
