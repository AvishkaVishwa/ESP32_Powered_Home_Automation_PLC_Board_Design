#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "web_server.h"
#include "auto_board.h"
#include "wifi_config.h"

static const char *TAG = "WEB_SERVER";

// WiFi credentials - MODIFY THESE!
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Global variables
static httpd_handle_t server = NULL;
static output_timer_t output_timers[NUM_OUTPUTS];
extern bool output_states[];
extern const gpio_num_t output_gpios[];  // Declare external GPIO array

// Manual control flags - when true, disable automatic input-to-output logic
static bool manual_control_active[NUM_OUTPUTS] = {false};
static uint32_t manual_control_timeout[NUM_OUTPUTS] = {0};
#define MANUAL_CONTROL_TIMEOUT_MS 300000  // 5 minutes timeout for manual control

// Simple HTML page with enhanced interactivity
static const char* simple_html_page = 
"<!DOCTYPE html>"
"<html><head><title>ESP32 Automation Board</title>"
"<meta name='viewport' content='width=device-width,initial-scale=1'><meta charset=\"UTF-8\">"
"<style>"
"* { box-sizing: border-box; }"
"body{font-family:'Segoe UI',Arial,sans-serif;margin:0;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;}"
".container{max-width:1200px;margin:0 auto;padding:20px;}"
"h1{text-align:center;color:white;font-size:2.5em;margin-bottom:10px;text-shadow:2px 2px 4px rgba(0,0,0,0.3);}"
".subtitle{text-align:center;color:rgba(255,255,255,0.9);margin-bottom:30px;font-size:1.1em;}"
".gpio-info{background:rgba(255,255,255,0.1);backdrop-filter:blur(10px);border-radius:15px;padding:15px;margin-bottom:20px;color:white;}"
".gpio-row{display:flex;justify-content:space-between;align-items:center;margin:8px 0;}"
".gpio-label{font-weight:bold;color:#ffd700;}"
".gpio-value{background:rgba(0,0,0,0.3);padding:4px 8px;border-radius:5px;font-family:monospace;}"
".outputs-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-bottom:30px;}"
".output{background:rgba(255,255,255,0.95);border-radius:15px;padding:20px;box-shadow:0 8px 32px rgba(0,0,0,0.2);transition:all 0.3s ease;position:relative;overflow:hidden;}"
".output:hover{transform:translateY(-5px);box-shadow:0 12px 40px rgba(0,0,0,0.3);}"
".output::before{content:'';position:absolute;top:0;left:0;right:0;height:4px;background:linear-gradient(90deg,#4CAF50,#45a049);transition:all 0.3s ease;}"
".output.off::before{background:linear-gradient(90deg,#f44336,#d32f2f);}"
".output-header{display:flex;justify-content:space-between;align-items:center;margin-bottom:15px;}"
".output-title{font-size:1.3em;font-weight:bold;color:#333;}"
".output-gpio{font-size:0.9em;color:#666;background:#f0f0f0;padding:4px 8px;border-radius:5px;font-family:monospace;}"
".status{font-weight:bold;margin:10px 0;padding:10px;border-radius:8px;text-align:center;transition:all 0.3s ease;}"
".status.on{background:linear-gradient(135deg,#4CAF50,#45a049);color:white;box-shadow:0 4px 15px rgba(76,175,80,0.4);animation:pulse-on 2s infinite;}"
".status.off{background:linear-gradient(135deg,#f44336,#d32f2f);color:white;box-shadow:0 4px 15px rgba(244,67,54,0.4);}"
"@keyframes pulse-on{0%,100%{box-shadow:0 4px 15px rgba(76,175,80,0.4);}50%{box-shadow:0 4px 25px rgba(76,175,80,0.8);}}"
"button{padding:12px 20px;margin:5px;border:none;border-radius:8px;cursor:pointer;font-weight:bold;transition:all 0.3s ease;position:relative;overflow:hidden;}"
"button:active{transform:scale(0.95);}"
"button::before{content:'';position:absolute;top:50%;left:50%;width:0;height:0;background:rgba(255,255,255,0.5);border-radius:50%;transition:all 0.3s ease;transform:translate(-50%,-50%);}"
"button:active::before{width:300px;height:300px;}"
".btn-on{background:linear-gradient(135deg,#4CAF50,#45a049);color:white;box-shadow:0 4px 15px rgba(76,175,80,0.3);}"
".btn-on:hover{background:linear-gradient(135deg,#45a049,#4CAF50);box-shadow:0 6px 20px rgba(76,175,80,0.4);transform:translateY(-2px);}"
".btn-off{background:linear-gradient(135deg,#f44336,#d32f2f);color:white;box-shadow:0 4px 15px rgba(244,67,54,0.3);}"
".btn-off:hover{background:linear-gradient(135deg,#d32f2f,#f44336);box-shadow:0 6px 20px rgba(244,67,54,0.4);transform:translateY(-2px);}"
".btn-timer{background:linear-gradient(135deg,#2196F3,#1976D2);color:white;box-shadow:0 4px 15px rgba(33,150,243,0.3);}"
".btn-timer:hover{background:linear-gradient(135deg,#1976D2,#2196F3);box-shadow:0 6px 20px rgba(33,150,243,0.4);transform:translateY(-2px);}"
".btn-cancel{background:linear-gradient(135deg,#ff9800,#f57c00);color:white;box-shadow:0 4px 15px rgba(255,152,0,0.3);}"
".btn-cancel:hover{background:linear-gradient(135deg,#f57c00,#ff9800);box-shadow:0 6px 20px rgba(255,152,0,0.4);transform:translateY(-2px);}"
".btn-settings{background:linear-gradient(135deg,#6c757d,#5a6268);color:white;box-shadow:0 4px 15px rgba(108,117,125,0.3);}"
".btn-settings:hover{background:linear-gradient(135deg,#5a6268,#6c757d);box-shadow:0 6px 20px rgba(108,117,125,0.4);transform:translateY(-2px);}"
"input[type=number]{padding:10px;border:2px solid #ddd;border-radius:8px;width:80px;font-size:14px;transition:all 0.3s ease;}"
"input[type=number]:focus{border-color:#2196F3;box-shadow:0 0 10px rgba(33,150,243,0.3);outline:none;}"
".timer-controls{display:flex;align-items:center;gap:10px;margin-top:15px;flex-wrap:wrap;}"
".timer-info{margin:10px 0;padding:10px;background:linear-gradient(135deg,#17a2b8,#138496);color:white;border-radius:8px;font-weight:bold;animation:timer-blink 1s infinite alternate;}"
"@keyframes timer-blink{0%{opacity:0.8;}100%{opacity:1;}}"
".realtime-corner{position:fixed;top:15px;right:15px;background:rgba(0,0,0,0.9);backdrop-filter:blur(10px);color:white;padding:15px;border-radius:12px;font-size:12px;z-index:1000;min-width:220px;box-shadow:0 8px 32px rgba(0,0,0,0.3);border:1px solid rgba(255,255,255,0.1);}"
".realtime-corner h4{margin:0 0 10px 0;font-size:14px;color:#4CAF50;text-align:center;}"
".realtime-corner div{margin:5px 0;display:flex;align-items:center;}"
".status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:8px;animation:status-pulse 2s infinite;}"
"@keyframes status-pulse{0%,100%{opacity:1;}50%{opacity:0.5;}}"
".status-connected{background:#4CAF50;}"
".status-disconnected{background:#f44336;}"
".status-warning{background:#ff9800;}"
".controls-section{text-align:center;margin:20px 0;}"
".refresh-btn{background:linear-gradient(135deg,#9C27B0,#7B1FA2);color:white;padding:12px 24px;font-size:16px;}"
".refresh-btn:hover{background:linear-gradient(135deg,#7B1FA2,#9C27B0);transform:translateY(-2px);}"
".connection-status{position:fixed;bottom:20px;left:20px;padding:10px 15px;border-radius:25px;color:white;font-weight:bold;z-index:1000;transition:all 0.3s ease;}"
".connection-status.connected{background:linear-gradient(135deg,#4CAF50,#45a049);}"
".connection-status.disconnected{background:linear-gradient(135deg,#f44336,#d32f2f);animation:shake 0.5s ease-in-out infinite;}"
"@keyframes shake{0%,100%{transform:translateX(0);}25%{transform:translateX(-5px);}75%{transform:translateX(5px);}}"
".loading{display:none;position:fixed;top:50%;left:50%;transform:translate(-50%,-50%);background:rgba(0,0,0,0.8);color:white;padding:20px;border-radius:10px;z-index:2000;}"
".spinner{border:3px solid rgba(255,255,255,0.3);border-top:3px solid white;border-radius:50%;width:30px;height:30px;animation:spin 1s linear infinite;margin:0 auto 10px;}"
"@keyframes spin{0%{transform:rotate(0deg);}100%{transform:rotate(360deg);}}"
"@media (max-width: 768px){.outputs-grid{grid-template-columns:1fr;}.realtime-corner{position:relative;top:auto;right:auto;margin-bottom:20px;}.timer-controls{justify-content:center;}}"
"</style></head><body>"
"<div class='loading' id='loadingModal'><div class='spinner'></div>Processing...</div>"
"<div class='connection-status connected' id='connectionIndicator'>🌐 Connected</div>"
"<div class='realtime-corner' id='realtimeCorner'>"
"<h4>📊 System Status</h4>"
"<div id='currentTime'>⏰ Loading...</div>"
"<div id='systemUptime'>⚡ Uptime: --</div>"
"<div id='connectionStatus'><span class='status-dot status-connected'></span>WiFi Connected</div>"
"<div id='memoryStatus'>💾 Memory: --</div>"
"<div id='activeTimers'>⏱️ Active: 0 timers</div>"
"</div>"
"<div class='container'>"
"<h1>🏠 ESP32 Automation Board</h1>"
"<p class='subtitle'>5-Channel 230V AC Control with Smart Timers</p>"
"<div class='gpio-info'>"
"<div style='text-align:center;margin-bottom:10px;font-weight:bold;color:#ffd700;'>📍 GPIO Pin Configuration</div>"
"<div class='gpio-row'><span class='gpio-label'>Inputs (12V-24V):</span><span class='gpio-value'>GPIO 4,5,18,19,21</span></div>"
"<div class='gpio-row'><span class='gpio-label'>Outputs (230V SSR):</span><span class='gpio-value'>GPIO 12,13,14,27,26</span></div>"
"<div class='gpio-row'><span class='gpio-label'>Status LED:</span><span class='gpio-value'>GPIO 2</span></div>"
"</div>"
"<div class='controls-section'>"
"<button onclick='location.href=\"/settings\"' class='btn-settings'>⚙️ WiFi Settings</button>"
"<button onclick='refreshAll()' class='refresh-btn'>🔄 Refresh All</button>"
"</div>"
"<div class='outputs-grid'>";

static const char* html_footer = 
"</div>"
"</div>"
"<script>"
"let isUpdating = false;"
"function showLoading(show) {"
"document.getElementById('loadingModal').style.display = show ? 'block' : 'none';"
"}"
"function updateConnectionIndicator(connected) {"
"const indicator = document.getElementById('connectionIndicator');"
"if (connected) {"
"indicator.className = 'connection-status connected';"
"indicator.innerHTML = 'Status: Connected';"
"} else {"
"indicator.className = 'connection-status disconnected';"
"indicator.innerHTML = 'Status: Connection Lost';"
"}}"
"function toggle(n) {"
"if (isUpdating) return;"
"isUpdating = true;"
"showLoading(true);"
"const btn = document.getElementById('btn'+n);"
"btn.style.opacity = '0.6';"
"fetch('/api/output/'+n+'/toggle',{method:'POST'})"
".then(response => {"
"if (!response.ok) throw new Error('Toggle failed');"
"updateStatus();"
"setTimeout(() => {"
"showLoading(false);"
"btn.style.opacity = '1';"
"isUpdating = false;"
"}, 500);"
"})"
".catch(e => {"
"console.error('Toggle error:',e);"
"showLoading(false);"
"btn.style.opacity = '1';"
"isUpdating = false;"
"updateConnectionIndicator(false);"
"});}"
"function setTimer(n) {"
"if (isUpdating) return;"
"const minutes = document.getElementById('timer'+n).value;"
"if (!minutes || minutes < 1 || minutes > 1440) {"
"alert('Warning: Please enter a valid time between 1-1440 minutes');"
"return;"
"}"
"isUpdating = true;"
"showLoading(true);"
"fetch('/api/output/'+n+'/timer',{method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({minutes:parseInt(minutes)})})"
".then(response => {"
"if (!response.ok) throw new Error('Timer set failed');"
"document.getElementById('timer'+n).value = '';"
"updateStatus();"
"setTimeout(() => {"
"showLoading(false);"
"isUpdating = false;"
"}, 500);"
"})"
".catch(e => {"
"console.error('Timer error:',e);"
"showLoading(false);"
"isUpdating = false;"
"updateConnectionIndicator(false);"
"});}"
"function cancelTimer(n) {"
"if (isUpdating) return;"
"isUpdating = true;"
"showLoading(true);"
"fetch('/api/output/'+n+'/cancel',{method:'POST'})"
".then(response => {"
"if (!response.ok) throw new Error('Cancel failed');"
"updateStatus();"
"setTimeout(() => {"
"showLoading(false);"
"isUpdating = false;"
"}, 500);"
"})"
".catch(e => {"
"console.error('Cancel error:',e);"
"showLoading(false);"
"isUpdating = false;"
"updateConnectionIndicator(false);"
"});}"
"function formatBytes(bytes) {"
"if (bytes < 1024) return bytes + ' B';"
"if (bytes < 1048576) return Math.round(bytes/1024) + ' KB';"
"return Math.round(bytes/1048576) + ' MB';"
"}"
"function formatUptime(seconds) {"
"const days = Math.floor(seconds / 86400);"
"const hours = Math.floor((seconds % 86400) / 3600);"
"const minutes = Math.floor((seconds % 3600) / 60);"
"if (days > 0) return days + 'd ' + hours + 'h ' + minutes + 'm';"
"if (hours > 0) return hours + 'h ' + minutes + 'm';"
"return minutes + 'm ' + (seconds % 60) + 's';"
"}"
"function updateRealTimeCorner() {"
"const now = new Date();"
"document.getElementById('currentTime').innerHTML = 'Time: ' + now.toLocaleTimeString();"
"}"
"function refreshAll() {"
"showLoading(true);"
"updateStatus();"
"setTimeout(() => showLoading(false), 1000);"
"}"
"function updateStatus() {"
"fetch('/api/status')"
".then(r => {"
"if (!r.ok) throw new Error('Status fetch failed');"
"return r.json();"
"})"
".then(data => {"
"updateConnectionIndicator(true);"
"for(let i=0; i<5; i++) {"
"const statusEl = document.getElementById('status'+(i+1));"
"const timerEl = document.getElementById('timer-info'+(i+1));"
"const btnEl = document.getElementById('btn'+(i+1));"
"const outputEl = document.querySelector('.output:nth-child('+(i+1)+')');"
"if(statusEl) {"
"statusEl.className = 'status ' + (data.outputs[i].state ? 'on' : 'off');"
"statusEl.textContent = data.outputs[i].state ? '🟢 ACTIVE' : '🔴 INACTIVE';"
"}"
"if(outputEl) {"
"outputEl.className = 'output ' + (data.outputs[i].state ? 'on' : 'off');"
"}"
"if(timerEl) {"
"if(data.outputs[i].timer_active) {"
"timerEl.innerHTML = '<div class=\"timer-info\">⏱️ Timer: ' + data.outputs[i].timer_remaining + ' min remaining</div>';"
"timerEl.style.display = 'block';"
"} else {"
"timerEl.innerHTML = '';"
"timerEl.style.display = 'none';"
"}"
"}"
"if(btnEl) {"
"btnEl.className = data.outputs[i].state ? 'btn-off' : 'btn-on';"
"btnEl.textContent = data.outputs[i].state ? '🔴 Turn OFF' : '🟢 Turn ON';"
"}"
"}"
"if(data.system) {"
"document.getElementById('systemUptime').innerHTML = '⚡ Uptime: ' + formatUptime(data.system.uptime_seconds);"
"document.getElementById('memoryStatus').innerHTML = '💾 Memory: ' + formatBytes(data.system.free_heap) + ' free';"
"document.getElementById('activeTimers').innerHTML = '⏱️ Active: ' + data.system.active_timers + ' timers';"
"const wifiStatus = data.system.wifi_connected ? 'WiFi Connected' : 'WiFi Disconnected';"
"const wifiClass = data.system.wifi_connected ? 'status-connected' : 'status-disconnected';"
"document.getElementById('connectionStatus').innerHTML = '<span class=\"status-dot ' + wifiClass + '\"></span>' + wifiStatus;"
"}"
"})"
".catch(e => {"
"console.error('Status update failed:',e);"
"updateConnectionIndicator(false);"
"});}"
"// Auto-update every 2 seconds"
"setInterval(updateStatus, 2000);"
"// Update real-time corner every second"
"setInterval(updateRealTimeCorner, 1000);"
"// Initial updates"
"updateStatus();"
"updateRealTimeCorner();"
"// Prevent double-clicks"
"document.addEventListener('click', function(e) {"
"if (e.target.tagName === 'BUTTON' && isUpdating) {"
"e.preventDefault();"
"e.stopPropagation();"
"}"
"});"
"</script></body></html>";

// Forward declarations
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t status_handler(httpd_req_t *req);
static esp_err_t toggle_handler(httpd_req_t *req);
static esp_err_t timer_handler(httpd_req_t *req);
static esp_err_t cancel_timer_handler(httpd_req_t *req);
static esp_err_t settings_handler(httpd_req_t *req);
static esp_err_t wifi_connect_handler(httpd_req_t *req);
static esp_err_t wifi_reset_handler(httpd_req_t *req);

// Helper function to get client IP address (simplified for ESP-IDF compatibility)
static const char* get_client_ip(httpd_req_t *req)
{
    // For ESP-IDF, we'll use a simple placeholder since getting client IP
    // from HTTP request is complex and not essential for functionality
    return "client";
}

// Web server statistics
typedef struct {
    uint32_t total_requests;
    uint32_t successful_requests;
    uint32_t failed_requests;
    uint32_t total_bytes_sent;
    uint32_t max_response_size;
    uint32_t min_response_size;
    uint32_t avg_response_time_ms;
} web_server_stats_t;

static web_server_stats_t server_stats = {0};

void print_web_server_stats(void)
{
    ESP_LOGI(TAG, "=== WEB SERVER STATISTICS ===");
    ESP_LOGI(TAG, "Total Requests: %lu", server_stats.total_requests);
    ESP_LOGI(TAG, "Successful: %lu, Failed: %lu", 
             server_stats.successful_requests, server_stats.failed_requests);
    ESP_LOGI(TAG, "Total Bytes Sent: %lu bytes", server_stats.total_bytes_sent);
    ESP_LOGI(TAG, "Max Response Size: %lu bytes", server_stats.max_response_size);
    ESP_LOGI(TAG, "Min Response Size: %lu bytes", server_stats.min_response_size);
    ESP_LOGI(TAG, "Avg Response Time: %lu ms", server_stats.avg_response_time_ms);
    ESP_LOGI(TAG, "Free Heap: %zu bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    ESP_LOGI(TAG, "============================");
}

void update_server_stats(uint32_t response_size, uint32_t response_time_ms, bool success)
{
    server_stats.total_requests++;
    if (success) {
        server_stats.successful_requests++;
        server_stats.total_bytes_sent += response_size;
        
        if (server_stats.max_response_size == 0 || response_size > server_stats.max_response_size) {
            server_stats.max_response_size = response_size;
        }
        if (server_stats.min_response_size == 0 || response_size < server_stats.min_response_size) {
            server_stats.min_response_size = response_size;
        }
        
        // Simple moving average for response time
        server_stats.avg_response_time_ms = 
            (server_stats.avg_response_time_ms + response_time_ms) / 2;
    } else {
        server_stats.failed_requests++;
    }
}

esp_err_t init_wifi_station(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize network interface
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    strcpy((char*)wifi_config.sta.ssid, WIFI_SSID);
    strcpy((char*)wifi_config.sta.password, WIFI_PASS);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi initialization completed. Connecting to %s...", WIFI_SSID);
    
    // Initialize timers
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        output_timers[i].is_active = false;
        output_timers[i].start_time = 0;
        output_timers[i].duration_minutes = 0;
        output_timers[i].output_num = i;
    }

    return ESP_OK;
}

static esp_err_t root_handler(httpd_req_t *req)
{
    uint32_t start_time = esp_timer_get_time() / 1000; // microseconds to milliseconds
    ESP_LOGI(TAG, "Root handler called");
    
    // Check available memory
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    ESP_LOGI(TAG, "Free heap: %zu bytes", free_heap);
    
    // Track total response size
    size_t total_response_size = 0;
    
    // Use chunked response to avoid large buffer allocation
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Connection", "close");
    
    // Send HTML header
    size_t header_size = strlen(simple_html_page);
    esp_err_t ret = httpd_resp_send_chunk(req, simple_html_page, header_size);
    total_response_size += header_size;
    ESP_LOGI(TAG, "Sent HTML header: %zu bytes", header_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send HTML header");
        return ret;
    }
    
    // Send each output block
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        const char* status_class = output_states[i] ? "on" : "off";
        const char* status_text = output_states[i] ? "ACTIVE" : "INACTIVE";
        const char* button_class = output_states[i] ? "btn-off" : "btn-on";
        const char* button_text = output_states[i] ? "Turn OFF" : "Turn ON";
        
        char timer_info[100] = "";
        if (output_timers[i].is_active) {
            uint32_t remaining = get_remaining_timer_minutes(i);
            snprintf(timer_info, sizeof(timer_info), "<p>Timer: %lu min remaining</p>", remaining);
        }
        
        // Use dynamic allocation to avoid format truncation warnings
        char *output_html = malloc(1536);  // Larger buffer for new design
        if (!output_html) {
            ESP_LOGE(TAG, "Failed to allocate memory for output HTML");
            return ESP_FAIL;
        }
        
        // Get GPIO pin numbers for display
        const gpio_num_t gpio_inputs[] = {GPIO_NUM_4, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_19, GPIO_NUM_21};
        const gpio_num_t gpio_outputs[] = {GPIO_NUM_12, GPIO_NUM_13, GPIO_NUM_14, GPIO_NUM_27, GPIO_NUM_26};
        
        snprintf(output_html, 1536,
            "<div class='output'>"
            "<div class='output-header'>"
            "<div class='output-title'>Output %d</div>"
            "<div class='output-gpio'>GPIO %d</div>"
            "</div>"
            "<div class='gpio-row'>"
            "<span class='gpio-label'>Input:</span>"
            "<span class='gpio-value'>GPIO %d (12V-24V)</span>"
            "</div>"
            "<div class='gpio-row'>"
            "<span class='gpio-label'>Output:</span>"
            "<span class='gpio-value'>GPIO %d (230V SSR)</span>"
            "</div>"
            "<div id='status%d' class='status %s'>%s</div>"
            "<div id='timer-info%d' style='%s'>%s</div>"
            "<button id='btn%d' class='%s' onclick='toggle(%d)'>%s</button>"
            "<div class='timer-controls'>"
            "<input type='number' id='timer%d' min='1' max='1440' placeholder='Minutes'>"
            "<button class='btn-timer' onclick='setTimer(%d)'>⏰ Set Timer</button>"
            "<button class='btn-cancel' onclick='cancelTimer(%d)'>❌ Cancel</button>"
            "</div>"
            "</div>",
            i + 1, // output number
            gpio_outputs[i], // output GPIO in header
            gpio_inputs[i], // input GPIO  
            gpio_outputs[i], // output GPIO repeated
            i + 1, // status id
            status_class, // status class
            status_text, // status text
            i + 1, // timer-info id
            output_timers[i].is_active ? "display:block" : "display:none", // timer visibility
            timer_info, // timer info text
            i + 1, // button id
            button_class, // button class
            i + 1, // toggle function parameter
            button_text, // button text
            i + 1, // timer input id
            i + 1, // setTimer function parameter
            i + 1); // cancelTimer function parameter
        
        ret = httpd_resp_send_chunk(req, output_html, strlen(output_html));
        size_t output_size = strlen(output_html);
        total_response_size += output_size;
        ESP_LOGI(TAG, "Sent output %d: %zu bytes", i + 1, output_size);
        
        free(output_html);  // Free the allocated memory
        
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send output %d", i);
            return ret;
        }
        
        // Small delay to let network stack process
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    
    // Send footer
    size_t footer_size = strlen(html_footer);
    ret = httpd_resp_send_chunk(req, html_footer, footer_size);
    total_response_size += footer_size;
    ESP_LOGI(TAG, "Sent footer: %zu bytes", footer_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send footer");
        return ret;
    }
    
    // End chunked response
    ret = httpd_resp_send_chunk(req, NULL, 0);
    
    uint32_t end_time = esp_timer_get_time() / 1000;
    uint32_t response_time = end_time - start_time;
    
    ESP_LOGI(TAG, "=== TOTAL HTTP RESPONSE SIZE: %zu bytes ===", total_response_size);
    ESP_LOGI(TAG, "=== RESPONSE TIME: %lu ms ===", response_time);
    ESP_LOGI(TAG, "Root handler completed: %s", ret == ESP_OK ? "success" : "failed");
    
    // Update statistics
    update_server_stats(total_response_size, response_time, ret == ESP_OK);
    
    return ret;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Status API request from %s", get_client_ip(req));
    
    // Build JSON response with current status of all outputs
    cJSON *json = cJSON_CreateObject();
    cJSON *outputs = cJSON_CreateArray();
    
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        cJSON *output = cJSON_CreateObject();
        cJSON_AddNumberToObject(output, "id", i + 1);
        cJSON_AddBoolToObject(output, "state", output_states[i]);
        cJSON_AddBoolToObject(output, "timer_active", output_timers[i].is_active);
        
        if (output_timers[i].is_active) {
            uint32_t remaining = get_remaining_timer_minutes(i);
            cJSON_AddNumberToObject(output, "timer_remaining", remaining);
            cJSON_AddNumberToObject(output, "timer_duration", output_timers[i].duration_minutes);
        } else {
            cJSON_AddNumberToObject(output, "timer_remaining", 0);
            cJSON_AddNumberToObject(output, "timer_duration", 0);
        }
        
        cJSON_AddItemToArray(outputs, output);
    }
    
    cJSON_AddItemToObject(json, "outputs", outputs);
    cJSON_AddStringToObject(json, "status", "ok");
    cJSON_AddNumberToObject(json, "timestamp", esp_timer_get_time() / 1000000);
    
    // Add system information for real-time corner
    cJSON *system_info = cJSON_CreateObject();
    cJSON_AddNumberToObject(system_info, "free_heap", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(system_info, "uptime_seconds", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(system_info, "total_requests", server_stats.total_requests);
    cJSON_AddBoolToObject(system_info, "wifi_connected", wifi_config_is_connected());
    
    // Count active timers
    int active_timers = 0;
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (output_timers[i].is_active) active_timers++;
    }
    cJSON_AddNumberToObject(system_info, "active_timers", active_timers);
    
    cJSON_AddItemToObject(json, "system", system_info);
    
    char *json_string = cJSON_Print(json);
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    esp_err_t ret = httpd_resp_send(req, json_string, strlen(json_string));
    
    ESP_LOGI(TAG, "Status API response sent: %zu bytes", strlen(json_string));
    
    free(json_string);
    cJSON_Delete(json);
    
    return ret;
}

static esp_err_t toggle_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Toggle handler called with URI: %s", req->uri);
    
    const char* uri = req->uri;
    char* output_num_str = strstr(uri, "/output/");
    if (output_num_str) {
        output_num_str += 8; // Skip "/output/"
        int output_num = atoi(output_num_str) - 1;
        
        ESP_LOGI(TAG, "Parsed output number: %d (from URI: %s)", output_num, req->uri);
        
        if (output_num >= 0 && output_num < NUM_OUTPUTS) {
            bool new_state = !output_states[output_num];
            ESP_LOGI(TAG, "Toggling Output %d from %s to %s", 
                     output_num + 1, 
                     output_states[output_num] ? "ON" : "OFF",
                     new_state ? "ON" : "OFF");
            
            web_set_output(output_num, new_state);
            httpd_resp_send(req, "OK", 2);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Invalid output number: %d", output_num);
        }
    } else {
        ESP_LOGE(TAG, "Could not find /output/ in URI: %s", req->uri);
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid output");
    return ESP_FAIL;
}

static esp_err_t timer_handler(httpd_req_t *req)
{
    char content[100];
    size_t content_len = req->content_len < sizeof(content) - 1 ? req->content_len : sizeof(content) - 1;
    
    if (httpd_req_recv(req, content, content_len) <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    content[content_len] = '\0';
    
    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *minutes_json = cJSON_GetObjectItem(json, "minutes");
    if (!cJSON_IsNumber(minutes_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid minutes");
        return ESP_FAIL;
    }
    
    int minutes = (int)cJSON_GetNumberValue(minutes_json);
    cJSON_Delete(json);
    
    const char* uri = req->uri;
    char* output_num_str = strstr(uri, "/output/");
    if (output_num_str) {
        output_num_str += 8;
        int output_num = atoi(output_num_str) - 1;
        
        if (output_num >= 0 && output_num < NUM_OUTPUTS && minutes > 0 && minutes <= MAX_TIMER_DURATION_MINUTES) {
            web_set_output_timer(output_num, minutes);
            httpd_resp_send(req, "OK", 2);
            return ESP_OK;
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid parameters");
    return ESP_FAIL;
}

static esp_err_t cancel_timer_handler(httpd_req_t *req)
{
    const char* uri = req->uri;
    char* output_num_str = strstr(uri, "/output/");
    if (output_num_str) {
        output_num_str += 8;
        int output_num = atoi(output_num_str) - 1;
        
        if (output_num >= 0 && output_num < NUM_OUTPUTS) {
            web_cancel_timer(output_num);
            httpd_resp_send(req, "OK", 2);
            return ESP_OK;
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid output");
    return ESP_FAIL;
}

static esp_err_t settings_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Settings page request from %s", get_client_ip(req));
    
    // Get current WiFi credentials
    wifi_credentials_t credentials;
    bool has_credentials = (wifi_config_load_credentials(&credentials) == ESP_OK);
    
    const char *settings_html = 
        "<!DOCTYPE html><html><head><title>ESP32 AutoBoard - Settings</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'><meta charset=\"UTF-8\">"
        "<style>"
        "body{font-family:Arial;margin:20px;background:#f0f0f0;}"
        ".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
        "h1{color:#333;text-align:center;}"
        "h2{color:#555;border-bottom:2px solid #007bff;padding-bottom:10px;}"
        ".form-group{margin:15px 0;}"
        "label{display:block;margin-bottom:5px;font-weight:bold;color:#333;}"
        "input[type=text],input[type=password],select{width:100%;padding:10px;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;}"
        "button{background:#007bff;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;margin:5px;}"
        "button:hover{background:#0056b3;}"
        ".btn-danger{background:#dc3545;}"
        ".btn-danger:hover{background:#c82333;}"
        ".btn-scan{background:#28a745;}"
        ".btn-scan:hover{background:#218838;}"
        ".current-wifi{background:#e7f3ff;padding:15px;border-radius:5px;margin:10px 0;}"
        ".network-list{max-height:200px;overflow-y:auto;border:1px solid #ddd;border-radius:5px;margin:10px 0;}"
        ".network-item{padding:10px;border-bottom:1px solid #eee;cursor:pointer;}"
        ".network-item:hover{background:#f8f9fa;}"
        ".back-link{text-align:center;margin-top:20px;}"
        ".status{margin:10px 0;padding:10px;border-radius:5px;}"
        ".status.success{background:#d4edda;color:#155724;border:1px solid #c3e6cb;}"
        ".status.error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb;}"
        "</style></head><body>"
        "<div class='container'>"
        "<h1>ESP32 AutoBoard Settings</h1>";
    
    // Send initial HTML
    httpd_resp_send_chunk(req, settings_html, strlen(settings_html));
    
    // WiFi Status Section
    char *wifi_status = malloc(512);
    if (wifi_config_is_connected()) {
        if (has_credentials) {
            snprintf(wifi_status, 512,
                "<h2>WiFi Configuration</h2>"
                "<div class='current-wifi'>"
                "<h3>Current Connection</h3>"
                "<p><strong>Status:</strong> Connected</p>"
                "<p><strong>SSID:</strong> %s</p>"
                "</div>",
                credentials.ssid
            );
        } else {
            snprintf(wifi_status, 512,
                "<h2>WiFi Configuration</h2>"
                "<div class='current-wifi'>"
                "<h3>Current Connection</h3>"
                "<p><strong>Status:</strong> Connected</p>"
                "</div>"
            );
        }
    } else {
        snprintf(wifi_status, 512,
            "<h2>WiFi Configuration</h2>"
            "<div class='current-wifi'>"
            "<h3>Current Connection</h3>"
            "<p><strong>Status:</strong> Not Connected</p>"
            "</div>"
        );
    }
    httpd_resp_send_chunk(req, wifi_status, strlen(wifi_status));
    free(wifi_status);
    
    // WiFi Configuration Form
    const char *wifi_form = 
        "<form onsubmit='return connectWifi(event)'>"
        "<div class='form-group'>"
        "<label for='ssid'>WiFi SSID:</label>"
        "<input type='text' id='ssid' name='ssid' required maxlength='31' placeholder='Enter WiFi network name'>"
        "</div>"
        "<div class='form-group'>"
        "<label for='password'>WiFi Password:</label>"
        "<input type='password' id='password' name='password' maxlength='63' placeholder='Enter WiFi password'>"
        "</div>"
        "<button type='submit'>Connect to WiFi</button>"
        "<button type='button' class='btn-danger' onclick='resetWifi()'>Reset WiFi Settings</button>"
        "</form>"
        "<div id='status'></div>";
    
    httpd_resp_send_chunk(req, wifi_form, strlen(wifi_form));
    
    // JavaScript and closing tags
    const char *settings_js = 
        "<script>"
        "function connectWifi(event){"
        "event.preventDefault();"
        "const ssid=document.getElementById('ssid').value;"
        "const password=document.getElementById('password').value;"
        "if(!ssid.trim()){"
        "document.getElementById('status').innerHTML='<div class=\"status error\">Please enter WiFi SSID</div>';"
        "return false;}"
        "document.getElementById('status').innerHTML='<div class=\"status\">Connecting to '+ssid+'...</div>';"
        "fetch('/api/wifi/connect',{method:'POST',headers:{'Content-Type':'application/json'},"
        "body:JSON.stringify({ssid:ssid,password:password})})"
        ".then(r=>r.json()).then(data=>{"
        "if(data.success){"
        "document.getElementById('status').innerHTML='<div class=\"status success\">Connected! Page will reload in 5 seconds...</div>';"
        "setTimeout(()=>location.reload(),5000);"
        "}else{"
        "document.getElementById('status').innerHTML='<div class=\"status error\">Connection failed: '+data.message+'</div>';"
        "}}).catch(e=>{"
        "console.error('Connection error:',e);"
        "document.getElementById('status').innerHTML='<div class=\"status error\">Request failed - check network</div>';"
        "});return false;}"
        "function resetWifi(){"
        "if(confirm('Reset WiFi settings? This will clear saved credentials.')){"
        "document.getElementById('status').innerHTML='<div class=\"status\">Resetting WiFi settings...</div>';"
        "fetch('/api/wifi/reset',{method:'POST'})"
        ".then(r=>r.json()).then(data=>{"
        "document.getElementById('status').innerHTML='<div class=\"status success\">WiFi settings reset</div>';"
        "setTimeout(()=>location.reload(),3000);"
        "}).catch(e=>{"
        "document.getElementById('status').innerHTML='<div class=\"status error\">Reset failed</div>';"
        "});}}"
        "</script>"
        "<div class='back-link'>"
        "<a href='/'>← Back to Control Panel</a>"
        "</div>"
        "</div></body></html>";
    
    esp_err_t ret = httpd_resp_send_chunk(req, settings_js, strlen(settings_js));
    httpd_resp_send_chunk(req, NULL, 0); // End chunked response
    
    ESP_LOGI(TAG, "Settings page sent successfully");
    return ret;
}

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WiFi connect request from %s", get_client_ip(req));
    
    char *buf = malloc(512);
    if (!buf) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
    int received = httpd_req_recv(req, buf, 511);
    if (received <= 0) {
        free(buf);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data received");
        return ESP_FAIL;
    }
    buf[received] = '\0';
    
    // Parse JSON request
    cJSON *json = cJSON_Parse(buf);
    free(buf);
    
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(json, "password");
    
    if (!cJSON_IsString(ssid_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID");
        return ESP_FAIL;
    }
    
    const char *ssid = cJSON_GetStringValue(ssid_json);
    const char *password = cJSON_IsString(password_json) ? cJSON_GetStringValue(password_json) : "";
    
    ESP_LOGI(TAG, "Attempting to connect to WiFi: %s", ssid);
    
    // Save credentials first
    wifi_credentials_t credentials;
    strncpy(credentials.ssid, ssid, WIFI_SSID_MAX_LEN - 1);
    strncpy(credentials.password, password, WIFI_PASS_MAX_LEN - 1);
    credentials.configured = true;
    
    esp_err_t save_ret = wifi_config_save_credentials(&credentials);
    
    // Prepare response BEFORE attempting connection to avoid timeout issues
    cJSON *response = cJSON_CreateObject();
    char *response_str = NULL;
    
    if (save_ret != ESP_OK) {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Failed to save credentials");
        ESP_LOGW(TAG, "Failed to save WiFi credentials");
    } else {
        // Try to connect (this may take time)
        esp_err_t connect_ret = wifi_config_connect_sta(ssid, password);
        
        if (connect_ret == ESP_OK) {
            cJSON_AddBoolToObject(response, "success", true);
            cJSON_AddStringToObject(response, "message", "Connected successfully");
            ESP_LOGI(TAG, "WiFi connection successful for SSID: %s", ssid);
        } else {
            cJSON_AddBoolToObject(response, "success", false);
            if (connect_ret == ESP_ERR_TIMEOUT) {
                cJSON_AddStringToObject(response, "message", "Connection timeout - check SSID and password");
            } else {
                cJSON_AddStringToObject(response, "message", "Connection failed - check credentials");
            }
            ESP_LOGW(TAG, "WiFi connection failed for SSID: %s (error: %s)", ssid, esp_err_to_name(connect_ret));
        }
    }
    
    response_str = cJSON_Print(response);
    
    // Send response with proper error handling
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
    esp_err_t ret = httpd_resp_send(req, response_str, strlen(response_str));
    
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to send WiFi connect response: %s", esp_err_to_name(ret));
    }
    
    free(response_str);
    cJSON_Delete(response);
    cJSON_Delete(json);
    
    return ret;
}

static esp_err_t wifi_reset_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "WiFi reset request from %s", get_client_ip(req));
    
    wifi_config_reset();
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddBoolToObject(response, "success", true);
    cJSON_AddStringToObject(response, "message", "WiFi settings reset");
    
    char *response_str = cJSON_Print(response);
    httpd_resp_set_type(req, "application/json");
    esp_err_t ret = httpd_resp_send(req, response_str, strlen(response_str));
    
    free(response_str);
    cJSON_Delete(response);
    
    ESP_LOGI(TAG, "WiFi settings reset completed");
    return ret;
}

// Web server task
void web_server_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Starting web server task");
    
    // Main loop - web server is started in main.c
    while (1) {
        // Handle timers and manual control
        process_timers();
        
        // Check manual control timeouts
        for (int i = 0; i < NUM_OUTPUTS; i++) {
            if (is_manual_control_active(i)) {
                // Manual control is active, keep output state
                continue;
            }
            
            // Manual control is not active, check if output should be turned off
            if (output_states[i]) {
                ESP_LOGI(TAG, "Turning OFF Output %d (manual control timeout)", i + 1);
                web_set_output(i, false);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    
    vTaskDelete(NULL);
}

esp_err_t start_web_server(void)
{
    // Check if server is already running
    if (server != NULL) {
        ESP_LOGW(TAG, "Web server is already running");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.max_uri_handlers = 22;  // Reduced: 1+1+15+1+2+2 = 22 handlers needed
    
    // Optimize for stability
    config.stack_size = 4096;
    config.max_resp_headers = 6;
    config.send_wait_timeout = 3;
    config.recv_wait_timeout = 3;
    config.lru_purge_enable = true;
    config.max_open_sockets = 2;
    config.keep_alive_enable = false;
    
    if (httpd_start(&server, &config) == ESP_OK) {
        // Root page
        httpd_uri_t root_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);
        
        // Status API
        httpd_uri_t status_uri = {
            .uri = "/api/status",
            .method = HTTP_GET,
            .handler = status_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &status_uri);
        
        // Register specific handlers for each output
        for (int i = 1; i <= NUM_OUTPUTS; i++) {
            char toggle_uri[32], timer_uri[32], cancel_uri[32];
            
            // Toggle URI
            snprintf(toggle_uri, sizeof(toggle_uri), "/api/output/%d/toggle", i);
            httpd_uri_t toggle = {
                .uri = strdup(toggle_uri),
                .method = HTTP_POST,
                .handler = toggle_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &toggle);
            ESP_LOGI(TAG, "Registered toggle URI: %s", toggle_uri);
            
            // Timer URI
            snprintf(timer_uri, sizeof(timer_uri), "/api/output/%d/timer", i);
            httpd_uri_t timer = {
                .uri = strdup(timer_uri),
                .method = HTTP_POST,
                .handler = timer_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &timer);
            ESP_LOGI(TAG, "Registered timer URI: %s", timer_uri);
            
            // Cancel URI
            snprintf(cancel_uri, sizeof(cancel_uri), "/api/output/%d/cancel", i);
            httpd_uri_t cancel = {
                .uri = strdup(cancel_uri),
                .method = HTTP_POST,
                .handler = cancel_timer_handler,
                .user_ctx = NULL
            };
            httpd_register_uri_handler(server, &cancel);
            ESP_LOGI(TAG, "Registered cancel URI: %s", cancel_uri);
        }
        
        // Settings page
        httpd_uri_t settings_uri = {
            .uri = "/settings",
            .method = HTTP_GET,
            .handler = settings_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &settings_uri);
        ESP_LOGI(TAG, "Registered settings URI: %s", "/settings");
        
        // WiFi connect API
        httpd_uri_t wifi_connect_uri = {
            .uri = "/api/wifi/connect",
            .method = HTTP_POST,
            .handler = wifi_connect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_connect_uri);
        ESP_LOGI(TAG, "Registered WiFi connect URI: %s", "/api/wifi/connect");
        
        // WiFi reset API
        httpd_uri_t wifi_reset_uri = {
            .uri = "/api/wifi/reset",
            .method = HTTP_POST,
            .handler = wifi_reset_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_reset_uri);
        ESP_LOGI(TAG, "Registered WiFi reset URI: %s", "/api/wifi/reset");
        
        ESP_LOGI(TAG, "Web server started on port %d", WEB_SERVER_PORT);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

esp_err_t stop_web_server(void)
{
    if (server == NULL) {
        ESP_LOGW(TAG, "Web server is not running");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping web server");
    esp_err_t ret = httpd_stop(server);
    if (ret == ESP_OK) {
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped successfully");
    } else {
        ESP_LOGE(TAG, "Failed to stop web server: %s", esp_err_to_name(ret));
    }
    return ret;
}

void web_set_output(uint8_t output_num, bool state)
{
    if (output_num < NUM_OUTPUTS) {
        ESP_LOGI(TAG, "Web: Setting Output %d to %s", output_num + 1, state ? "ON" : "OFF");
        
        set_output(output_num, state);
        
        // Set manual control flag and timeout - very aggressive
        manual_control_active[output_num] = true;
        manual_control_timeout[output_num] = esp_timer_get_time() / 1000;
        
        // Also log the current GPIO state for debugging
        ESP_LOGI(TAG, "Web: Output %d set to %s (Manual control active) - GPIO state: %d", 
                 output_num + 1, 
                 state ? "ON" : "OFF",
                 gpio_get_level(output_gpios[output_num]));
    }
}

void web_set_output_timer(uint8_t output_num, uint32_t duration_minutes)
{
    if (output_num < NUM_OUTPUTS && duration_minutes > 0 && duration_minutes <= MAX_TIMER_DURATION_MINUTES) {
        output_timers[output_num].is_active = true;
        output_timers[output_num].start_time = esp_timer_get_time() / 1000000;
        output_timers[output_num].duration_minutes = duration_minutes;
        output_timers[output_num].output_num = output_num;
        
        // Set manual control flag to prevent automatic input control
        manual_control_active[output_num] = true;
        manual_control_timeout[output_num] = esp_timer_get_time() / 1000;
        
        web_set_output(output_num, true);
        
        ESP_LOGI(TAG, "Web: Timer set for Output %d - %lu minutes", output_num + 1, duration_minutes);
    }
}

void web_cancel_timer(uint8_t output_num)
{
    if (output_num < NUM_OUTPUTS) {
        output_timers[output_num].is_active = false;
        output_timers[output_num].start_time = 0;
        output_timers[output_num].duration_minutes = 0;
        
        // Clear manual control flag to allow automatic input control again
        manual_control_active[output_num] = false;
        manual_control_timeout[output_num] = 0;
        
        ESP_LOGI(TAG, "Web: Timer cancelled for Output %d", output_num + 1);
    }
}

bool get_output_state(uint8_t output_num)
{
    if (output_num < NUM_OUTPUTS) {
        return output_states[output_num];
    }
    return false;
}

uint32_t get_remaining_timer_minutes(uint8_t output_num)
{
    if (output_num < NUM_OUTPUTS && output_timers[output_num].is_active) {
        uint32_t current_time = esp_timer_get_time() / 1000000;
        uint32_t elapsed_time = current_time - output_timers[output_num].start_time;
        uint32_t total_duration_seconds = output_timers[output_num].duration_minutes * 60;
        
        if (elapsed_time < total_duration_seconds) {
            uint32_t remaining_seconds = total_duration_seconds - elapsed_time;
            return (remaining_seconds + 59) / 60; // Round up to next minute
        }
    }
    return 0;
}

void process_timers(void)
{
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (output_timers[i].is_active) {
            uint32_t current_time = esp_timer_get_time() / 1000000;
            uint32_t elapsed_time = current_time - output_timers[i].start_time;
            uint32_t total_duration_seconds = output_timers[i].duration_minutes * 60;
            
            if (elapsed_time >= total_duration_seconds) {
                // Timer expired
                ESP_LOGI(TAG, "Timer expired for Output %d", i + 1);
                
                output_timers[i].is_active = false;
                output_timers[i].start_time = 0;
                output_timers[i].duration_minutes = 0;
                
                // Turn off the output
                web_set_output(i, false);
                
                // Clear manual control flag
                manual_control_active[i] = false;
                manual_control_timeout[i] = 0;
            }
        }
    }
}

bool is_manual_control_active(uint8_t output_num)
{
    if (output_num >= NUM_OUTPUTS) {
        return false;
    }
    
    if (!manual_control_active[output_num]) {
        return false;
    }
    
    // Check timeout
    uint32_t current_time = esp_timer_get_time() / 1000;
    if (current_time - manual_control_timeout[output_num] > MANUAL_CONTROL_TIMEOUT_MS) {
        manual_control_active[output_num] = false;
        manual_control_timeout[output_num] = 0;
        ESP_LOGI(TAG, "Manual control timeout for Output %d", output_num + 1);
        return false;
    }
    
    return true;
}

void web_server_monitor_task(void *arg)
{
    ESP_LOGI(TAG, "Web server monitor task started");
    
    uint32_t stats_counter = 0;
    
    while (1) {
        // Print server statistics every 60 seconds
        if (++stats_counter >= 60) {
            print_web_server_stats();
            stats_counter = 0;
        }
        
        // Check server health and restart if needed
        if (server == NULL) {
            ESP_LOGW(TAG, "Web server is not running, attempting to restart...");
            start_web_server();
        }
        
        // Check memory usage
        size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_8BIT);
        if (free_heap < 10240) { // Less than 10KB free
            ESP_LOGW(TAG, "Low memory warning: %zu bytes free", free_heap);
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000)); // Check every second
    }
}
