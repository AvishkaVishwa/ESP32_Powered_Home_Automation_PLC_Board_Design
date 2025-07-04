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

// Simple HTML page that is less likely to cause issues
static const char* simple_html_page = 
"<!DOCTYPE html>"
"<html><head><title>ESP32 Automation Board</title>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<style>"
"body{font-family:Arial;margin:20px;background:#f0f0f0;}"
".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:10px;}"
"h1{text-align:center;color:#333;}"
".output{border:1px solid #ddd;margin:10px 0;padding:15px;border-radius:5px;}"
".status{font-weight:bold;margin:10px 0;}"
".on{color:#4CAF50;}"
".off{color:#f44336;}"
"button{padding:8px 16px;margin:5px;border:none;border-radius:4px;cursor:pointer;}"
".btn-on{background:#4CAF50;color:white;}"
".btn-off{background:#f44336;color:white;}"
".btn-timer{background:#2196F3;color:white;}"
".btn-cancel{background:#ff9800;color:white;}"
"input{padding:5px;width:80px;}"
".realtime-corner{position:fixed;top:10px;right:10px;background:rgba(0,0,0,0.8);color:white;padding:10px;border-radius:8px;font-size:12px;z-index:1000;min-width:200px;}"
".realtime-corner h4{margin:0 0 8px 0;font-size:14px;color:#4CAF50;}"
".realtime-corner div{margin:3px 0;}"
".status-dot{display:inline-block;width:8px;height:8px;border-radius:50%;margin-right:6px;}"
".status-connected{background:#4CAF50;}"
".status-disconnected{background:#f44336;}"
".status-warning{background:#ff9800;}"
"</style></head><body>"
"<div class='realtime-corner' id='realtimeCorner'>"
"<h4>🌐 System Status</h4>"
"<div id='currentTime'>⏰ Loading...</div>"
"<div id='systemUptime'>⚡ Uptime: --</div>"
"<div id='connectionStatus'><span class='status-dot status-connected'></span>Connected</div>"
"<div id='memoryStatus'>💾 Memory: --</div>"
"<div id='activeTimers'>⏱️ Timers: 0 active</div>"
"</div>"
"<div class='container'>"
"<h1>ESP32 Automation Board</h1>"
"<p style='text-align:center;'>5 Outputs with Timer Control</p>"
"<div style='text-align:center;margin:10px;'>"
"<a href='/settings' style='background:#6c757d;color:white;padding:8px 16px;text-decoration:none;border-radius:5px;'>⚙️ WiFi Settings</a>"
"</div>";

static const char* html_footer = 
"<div style='text-align:center;margin:20px 0;'>"
"<button onclick='location.reload()' style='background:#9C27B0;color:white;padding:10px 20px;'>Refresh</button>"
"</div>"
"</div>"
"<script>"
"function toggle(n){fetch('/api/output/'+n+'/toggle',{method:'POST'})"
".then(()=>updateStatus());}"
"function setTimer(n){const m=document.getElementById('timer'+n).value;"
"if(m>0&&m<=1440){fetch('/api/output/'+n+'/timer',{method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify({minutes:parseInt(m)})})"
".then(()=>updateStatus());}else{"
"alert('Enter valid time (1-1440 minutes)');}}"
"function cancelTimer(n){fetch('/api/output/'+n+'/cancel',{method:'POST'})"
".then(()=>updateStatus());}"
"function formatBytes(bytes){"
"if(bytes<1024) return bytes + ' B';"
"if(bytes<1048576) return Math.round(bytes/1024) + ' KB';"
"return Math.round(bytes/1048576) + ' MB';}"
"function formatUptime(seconds){"
"const hours = Math.floor(seconds / 3600);"
"const minutes = Math.floor((seconds % 3600) / 60);"
"const secs = seconds % 60;"
"return hours + 'h ' + minutes + 'm ' + secs + 's';}"
"function updateRealTimeCorner(){"
"const now = new Date();"
"document.getElementById('currentTime').innerHTML = '⏰ ' + now.toLocaleTimeString();"
"}"
"function updateStatus(){"
"fetch('/api/status')"
".then(r=>r.json()).then(data=>{"
"updateConnectionStatus(true);"
"for(let i=0;i<5;i++){"
"const statusEl=document.getElementById('status'+(i+1));"
"const timerEl=document.getElementById('timer-info'+(i+1));"
"const btnEl=document.getElementById('btn'+(i+1));"
"if(statusEl){statusEl.className='status '+(data.outputs[i].state?'on':'off');"
"statusEl.textContent='Status: '+(data.outputs[i].state?'ON':'OFF');}"
"if(timerEl){timerEl.innerHTML=data.outputs[i].timer_active?"
"'<p>Timer: '+data.outputs[i].timer_remaining+' min remaining</p>':'';}"
"if(btnEl){btnEl.className=data.outputs[i].state?'btn-off':'btn-on';"
"btnEl.textContent=data.outputs[i].state?'Turn OFF':'Turn ON';}}"
"if(data.system){"
"document.getElementById('systemUptime').innerHTML = '⚡ Uptime: ' + formatUptime(data.system.uptime_seconds);"
"document.getElementById('memoryStatus').innerHTML = '💾 Memory: ' + formatBytes(data.system.free_heap) + ' free';"
"document.getElementById('activeTimers').innerHTML = '⏱️ Timers: ' + data.system.active_timers + ' active';"
"const wifiStatus = data.system.wifi_connected ? 'Connected' : 'Disconnected';"
"const wifiClass = data.system.wifi_connected ? 'status-connected' : 'status-disconnected';"
"document.getElementById('connectionStatus').innerHTML = '<span class=\"status-dot ' + wifiClass + '\"></span>' + wifiStatus;"
"}}).catch(e=>{"
"console.error('Status update failed:',e);"
"updateConnectionStatus(false);"
"});}"
"function updateConnectionStatus(connected){"
"if(!connected){"
"document.getElementById('connectionStatus').innerHTML = '<span class=\"status-dot status-disconnected\"></span>API Error';"
"}}"
"setInterval(updateStatus,2000);"
"setInterval(updateRealTimeCorner,1000);"
"updateStatus();"
"updateRealTimeCorner();"
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
        const char* status_text = output_states[i] ? "ON" : "OFF";
        const char* button_class = output_states[i] ? "btn-off" : "btn-on";
        const char* button_text = output_states[i] ? "Turn OFF" : "Turn ON";
        
        char timer_info[100] = "";
        if (output_timers[i].is_active) {
            uint32_t remaining = get_remaining_timer_minutes(i);
            snprintf(timer_info, sizeof(timer_info), "<p>Timer: %lu min remaining</p>", remaining);
        }
        
        // Use dynamic allocation to avoid format truncation warnings
        char *output_html = malloc(1024);  // Large enough buffer
        if (!output_html) {
            ESP_LOGE(TAG, "Failed to allocate memory for output HTML");
            return ESP_FAIL;
        }
        
        snprintf(output_html, 1024,
            "<div class='output'>"
            "<h3>Output %d (230V AC)</h3>"
            "<div id='status%d' class='status %s'>Status: %s</div>"
            "<div id='timer-info%d'>%s</div>"
            "<button id='btn%d' class='%s' onclick='toggle(%d)'>%s</button>"
            "<div style='margin-top:10px;'>"
            "<input type='number' id='timer%d' min='1' max='1440' placeholder='Min'>"
            "<button class='btn-timer' onclick='setTimer(%d)'>Set Timer</button>"
            "<button class='btn-cancel' onclick='cancelTimer(%d)'>Cancel</button>"
            "</div>"
            "</div>",
            i + 1, i + 1, status_class, status_text, i + 1, timer_info,
            i + 1, button_class, i + 1, button_text,
            i + 1, i + 1, i + 1);
        
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
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
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
