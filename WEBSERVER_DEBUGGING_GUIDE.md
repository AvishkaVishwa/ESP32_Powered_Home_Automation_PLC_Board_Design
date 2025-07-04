# ESP32 Web Server AC Output Control - Debugging Guide

## Problem Summary
AC output control via web interface doesn't work despite correct HTTP endpoints and handlers.

## Root Causes Identified

### 1. **Automatic Control Task Override**
- `output_control_task` in `auto_board_tasks.c` runs every 100ms
- It forces outputs to match inverted input states
- This overrides any manual web control attempts
- **FIXED**: Added manual control flags to disable automatic control when web interface is used

### 2. **JavaScript Index Mismatch**
- Web UI buttons call `toggle(i)` but should call `toggle(i+1)`
- API expects 1-based indexing but was receiving 0-based
- **FIXED**: Updated button onclick handlers to use `i+1`

### 3. **Format Truncation Warning**
- `snprintf` buffer was too small causing compilation warnings
- **FIXED**: Used dynamic allocation with 1024-byte buffer

## Solutions Implemented

### Manual Control System
```c
// 5-minute timeout for manual control
#define MANUAL_CONTROL_TIMEOUT_MS 300000

static bool manual_control_active[NUM_OUTPUTS] = {false};
static uint32_t manual_control_timeout[NUM_OUTPUTS] = {0};
```

### Enhanced Logging
- Added detailed logging in `toggle_handler`
- Shows URI parsing and GPIO state changes
- Helps debug communication issues

### Output Control Task Modification
```c
// Only apply automatic control if no manual/timer control is active
if (!timer_active && !manual_active) {
    // Apply input-to-output logic
}
```

## Testing Instructions

### 1. Monitor Serial Output
```bash
# Look for these log messages:
Web: Setting Output X to ON/OFF
Toggle handler called with URI: /api/output/X/toggle
Parsed output number: X
```

### 2. Test Web Interface
1. Open browser to ESP32 IP address
2. Click toggle buttons for each output
3. Check serial logs for GPIO state changes
4. Verify outputs physically turn on/off

### 3. Test Timer Functionality
1. Enter timer value (1-1440 minutes)
2. Click "Set Timer" button
3. Verify output turns ON and timer starts
4. Check remaining time display

## Hardware Considerations

### ESP32 vs ESP32-S3
Your question about ESP32-S3: **The microcontroller type is NOT the issue.**

Both ESP32 and ESP32-S3 can handle this web server functionality. The problem is software-based:
- Task scheduling conflicts
- Index mismatches in JavaScript
- Buffer overflow warnings

### GPIO Configuration
Verify these GPIO pins in `auto_board.h`:
```c
#define OUTPUT_1_GPIO   GPIO_NUM_12
#define OUTPUT_2_GPIO   GPIO_NUM_13
#define OUTPUT_3_GPIO   GPIO_NUM_14
#define OUTPUT_4_GPIO   GPIO_NUM_27
#define OUTPUT_5_GPIO   GPIO_NUM_26
```

## Troubleshooting Steps

### If Outputs Still Don't Work:

1. **Check GPIO Physical Connection**
   ```c
   // Add to main.c for testing:
   gpio_set_level(OUTPUT_1_GPIO, 1); // Force ON
   vTaskDelay(1000);
   gpio_set_level(OUTPUT_1_GPIO, 0); // Force OFF
   ```

2. **Disable Automatic Control Completely**
   ```c
   // Comment out this line in main.c:
   // xTaskCreate(output_control_task, "output_control_task", 4096, NULL, 8, NULL);
   ```

3. **Test Direct GPIO Control**
   ```c
   // In web_set_output function, add:
   ESP_LOGI(TAG, "Before: GPIO %d = %d", output_gpios[output_num], gpio_get_level(output_gpios[output_num]));
   gpio_set_level(output_gpios[output_num], state ? 1 : 0);
   ESP_LOGI(TAG, "After: GPIO %d = %d", output_gpios[output_num], gpio_get_level(output_gpios[output_num]));
   ```

## Expected Behavior After Fixes

1. **Web Toggle**: Click button → GPIO immediately changes → LED/SSR responds
2. **Timer Control**: Set timer → Output turns ON → Countdown works → Auto OFF after timer
3. **Manual Override**: Web control overrides automatic input-to-output logic for 5 minutes
4. **Logging**: Detailed logs show every step of the process

## Performance Monitoring

The web server now includes:
- Response size tracking
- Memory usage monitoring  
- Request statistics
- Chunked HTTP responses for stability

## Conclusion

The AC output control issue was primarily caused by task conflicts, not hardware limitations. The ESP32 is perfectly capable of handling this application. The fixes implemented should resolve the control issues while maintaining system stability.
