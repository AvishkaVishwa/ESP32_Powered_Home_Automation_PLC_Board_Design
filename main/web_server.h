#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>
#include <stdint.h>

// Web server configuration
#define WEB_SERVER_PORT 80
#define MAX_TIMER_DURATION_MINUTES 1440  // 24 hours max

// Timer structure for outputs
typedef struct {
    bool is_active;
    uint32_t start_time;
    uint32_t duration_minutes;
    uint8_t output_num;
} output_timer_t;

// Function prototypes
esp_err_t init_wifi_station(void);
esp_err_t start_web_server(void);
esp_err_t stop_web_server(void);
void web_set_output(uint8_t output_num, bool state);
void web_set_output_timer(uint8_t output_num, uint32_t duration_minutes);
void web_cancel_timer(uint8_t output_num);
bool get_output_state(uint8_t output_num);
uint32_t get_remaining_timer_minutes(uint8_t output_num);
void process_timers(void);
bool is_manual_control_active(uint8_t output_num);
void web_server_monitor_task(void *arg);

// WiFi credentials (you should modify these)
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

#endif // WEB_SERVER_H
