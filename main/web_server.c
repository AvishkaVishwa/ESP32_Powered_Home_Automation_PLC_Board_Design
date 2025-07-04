#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "web_server.h"
#include "auto_board.h"
#include "wifi_config.h"

// Define MIN macro if not available
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

// Fallback definitions for IntelliSense compatibility
#ifndef CONFIG_LOG_MAXIMUM_LEVEL
#define CONFIG_LOG_MAXIMUM_LEVEL ESP_LOG_VERBOSE
#endif

#ifndef CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM
#define CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM 10
#endif

#ifndef CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM
#define CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM 32
#endif

#ifndef CONFIG_ESP_WIFI_TX_BUFFER_TYPE
#define CONFIG_ESP_WIFI_TX_BUFFER_TYPE 1
#endif

#ifndef CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM
#define CONFIG_ESP_WIFI_STATIC_TX_BUFFER_NUM 16
#endif

#ifndef CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM
#define CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM 32
#endif

#ifndef CONFIG_ESP_WIFI_RX_BA_WIN
#define CONFIG_ESP_WIFI_RX_BA_WIN 6
#endif

#ifndef CONFIG_ESP_WIFI_TX_BA_WIN
#define CONFIG_ESP_WIFI_TX_BA_WIN 6
#endif

#ifndef CONFIG_ESP_WIFI_DYNAMIC_RX_MGMT_BUF
#define CONFIG_ESP_WIFI_DYNAMIC_RX_MGMT_BUF 5
#endif

#ifndef CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM
#define CONFIG_ESP_WIFI_ESPNOW_MAX_ENCRYPT_NUM 7
#endif

static const char *TAG = "WEB_SERVER";

// WiFi credentials - MODIFY THESE!
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Global variables
static httpd_handle_t server = NULL;
static output_timer_t output_timers[NUM_OUTPUTS];
extern bool output_states[];

// HTML page content
static const char* html_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"    <title>ESP32 Automation Board</title>"
"    <meta charset='utf-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1'>"
"    <style>"
"        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }"
"        .container { max-width: 800px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }"
"        h1 { color: #333; text-align: center; }"
"        .output-control { border: 1px solid #ddd; margin: 10px 0; padding: 15px; border-radius: 5px; }"
"        .output-header { font-weight: bold; font-size: 18px; margin-bottom: 10px; }"
"        .control-row { display: flex; align-items: center; margin: 10px 0; }"
"        .control-row label { min-width: 100px; }"
"        button { padding: 8px 16px; margin: 5px; border: none; border-radius: 4px; cursor: pointer; }"
"        .btn-on { background-color: #4CAF50; color: white; }"
"        .btn-off { background-color: #f44336; color: white; }"
"        .btn-timer { background-color: #2196F3; color: white; }"
"        .btn-cancel { background-color: #ff9800; color: white; }"
"        input[type='number'] { padding: 5px; width: 80px; }"
"        .status { font-weight: bold; margin-left: 10px; }"
"        .status.on { color: #4CAF50; }"
"        .status.off { color: #f44336; }"
"        .timer-info { color: #2196F3; font-style: italic; }"
"        .refresh-btn { background-color: #9C27B0; color: white; padding: 10px 20px; font-size: 16px; }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>ESP32 Automation Board</h1>"
"        <p style='text-align: center; color: #666;'>Control 230V AC Outputs with Timer Functionality</p>"
"        "
"        <div style='text-align: center; margin: 20px 0;'>"
"            <button class='refresh-btn' onclick='location.reload()'>Refresh Status</button>"
"            <button class='refresh-btn' onclick='location.href=\"/wifi\"' style='margin-left: 10px; background-color: #2196F3;'>WiFi Settings</button>"
"        </div>"
"        "
"        <div id='outputs'>"
"            <!-- Output controls will be loaded here -->"
"        </div>"
"    </div>"
"    "
"    <script>"
"        function toggleOutput(num) {"
"            fetch('/api/output/' + num + '/toggle', {method: 'POST'})"
"                .then(() => setTimeout(() => location.reload(), 500));"
"        }"
"        "
"        function setTimer(num) {"
"            const minutes = document.getElementById('timer' + num).value;"
"            if (minutes > 0 && minutes <= 1440) {"
"                fetch('/api/output/' + num + '/timer', {"
"                    method: 'POST',"
"                    headers: {'Content-Type': 'application/json'},"
"                    body: JSON.stringify({minutes: parseInt(minutes)})"
"                }).then(() => setTimeout(() => location.reload(), 500));"
"            } else {"
"                alert('Please enter a valid time (1-1440 minutes)');"
"            }"
"        }"
"        "
"        function cancelTimer(num) {"
"            fetch('/api/output/' + num + '/cancel', {method: 'POST'})"
"                .then(() => setTimeout(() => location.reload(), 500));"
"        }"
"        "
"        // Auto-refresh every 30 seconds"
"        setInterval(() => location.reload(), 30000);"
"    </script>"
"</body>"
"</html>";

// WiFi Configuration HTML page
static const char* wifi_config_page = 
"<!DOCTYPE html>"
"<html>"
"<head>"
"    <title>ESP32 WiFi Configuration</title>"
"    <meta charset='utf-8'>"
"    <meta name='viewport' content='width=device-width, initial-scale=1'>"
"    <style>"
"        body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }"
"        .container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 6px rgba(0,0,0,0.1); }"
"        h1 { color: #333; text-align: center; }"
"        .form-group { margin: 15px 0; }"
"        label { display: block; margin-bottom: 5px; font-weight: bold; }"
"        input[type='text'], input[type='password'], select { width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }"
"        button { background-color: #4CAF50; color: white; padding: 12px 20px; border: none; border-radius: 4px; cursor: pointer; width: 100%; font-size: 16px; margin: 10px 0; }"
"        button:hover { background-color: #45a049; }"
"        .btn-scan { background-color: #2196F3; }"
"        .btn-scan:hover { background-color: #1976D2; }"
"        .btn-back { background-color: #ff9800; }"
"        .btn-back:hover { background-color: #f57c00; }"
"        .network-list { max-height: 200px; overflow-y: auto; border: 1px solid #ddd; border-radius: 4px; margin: 10px 0; }"
"        .network-item { padding: 10px; border-bottom: 1px solid #eee; cursor: pointer; display: flex; justify-content: space-between; }"
"        .network-item:hover { background-color: #f5f5f5; }"
"        .network-item:last-child { border-bottom: none; }"
"        .signal-strength { font-size: 12px; color: #666; }"
"        .status { padding: 10px; border-radius: 4px; margin: 10px 0; text-align: center; }"
"        .status.success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }"
"        .status.error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }"
"        .status.info { background-color: #d1ecf1; color: #0c5460; border: 1px solid #bee5eb; }"
"    </style>"
"</head>"
"<body>"
"    <div class='container'>"
"        <h1>WiFi Configuration</h1>"
"        <div id='status'></div>"
"        "
"        <div class='form-group'>"
"            <button class='btn-scan' onclick='scanNetworks()'>Scan WiFi Networks</button>"
"        </div>"
"        "
"        <div id='networks' class='network-list' style='display:none;'></div>"
"        "
"        <form id='wifiForm'>"
"            <div class='form-group'>"
"                <label for='ssid'>WiFi Network (SSID):</label>"
"                <input type='text' id='ssid' name='ssid' required>"
"            </div>"
"            "
"            <div class='form-group'>"
"                <label for='password'>Password:</label>"
"                <input type='password' id='password' name='password' required>"
"            </div>"
"            "
"            <button type='submit'>Connect to WiFi</button>"
"        </form>"
"        "
"        <button class='btn-back' onclick='goToMainPage()'>Back to Main Page</button>"
"    </div>"
"    "
"    <script>"
"        function scanNetworks() {"
"            document.getElementById('status').innerHTML = '<div class=\"status info\">Scanning networks...</div>';"
"            fetch('/api/wifi/scan')"
"                .then(response => response.json())"
"                .then(data => {"
"                    const networksDiv = document.getElementById('networks');"
"                    networksDiv.innerHTML = '';"
"                    if (data.networks && data.networks.length > 0) {"
"                        data.networks.forEach(network => {"
"                            const networkDiv = document.createElement('div');"
"                            networkDiv.className = 'network-item';"
"                            networkDiv.onclick = () => selectNetwork(network.ssid);"
"                            const signalStrength = network.rssi > -50 ? 'Excellent' : network.rssi > -60 ? 'Good' : network.rssi > -70 ? 'Fair' : 'Weak';"
"                            networkDiv.innerHTML = `<span>${network.ssid}</span><span class='signal-strength'>${signalStrength} (${network.rssi} dBm)</span>`;"
"                            networksDiv.appendChild(networkDiv);"
"                        });"
"                        networksDiv.style.display = 'block';"
"                        document.getElementById('status').innerHTML = '<div class=\"status success\">Found ' + data.networks.length + ' networks</div>';"
"                    } else {"
"                        document.getElementById('status').innerHTML = '<div class=\"status error\">No networks found</div>';"
"                    }"
"                })"
"                .catch(error => {"
"                    document.getElementById('status').innerHTML = '<div class=\"status error\">Scan failed: ' + error.message + '</div>';"
"                });"
"        }"
"        "
"        function selectNetwork(ssid) {"
"            document.getElementById('ssid').value = ssid;"
"        }"
"        "
"        function goToMainPage() {"
"            window.location.href = '/';"
"        }"
"        "
"        document.getElementById('wifiForm').onsubmit = function(e) {"
"            e.preventDefault();"
"            const ssid = document.getElementById('ssid').value;"
"            const password = document.getElementById('password').value;"
"            "
"            document.getElementById('status').innerHTML = '<div class=\"status info\">Connecting to ' + ssid + '...</div>';"
"            "
"            fetch('/api/wifi/connect', {"
"                method: 'POST',"
"                headers: {'Content-Type': 'application/json'},"
"                body: JSON.stringify({ssid: ssid, password: password})"
"            })"
"            .then(response => response.json())"
"            .then(data => {"
"                if (data.success) {"
"                    document.getElementById('status').innerHTML = '<div class=\"status success\">Connected successfully! Redirecting to main page...</div>';"
"                    setTimeout(() => window.location.href = '/', 3000);"
"                } else {"
"                    document.getElementById('status').innerHTML = '<div class=\"status error\">Connection failed: ' + (data.message || 'Unknown error') + '</div>';"
"                }"
"            })"
"            .catch(error => {"
"                document.getElementById('status').innerHTML = '<div class=\"status error\">Connection failed: ' + error.message + '</div>';"
"            });"
"        }"
"    </script>"
"</body>"
"</html>";

// Forward declarations
static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static esp_err_t root_handler(httpd_req_t *req);
static esp_err_t status_handler(httpd_req_t *req);
static esp_err_t toggle_handler(httpd_req_t *req);
static esp_err_t timer_handler(httpd_req_t *req);
static esp_err_t cancel_timer_handler(httpd_req_t *req);
static esp_err_t wifi_scan_handler(httpd_req_t *req);
static esp_err_t wifi_config_page_handler(httpd_req_t *req);
static esp_err_t wifi_connect_handler(httpd_req_t *req);

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

    // Register event handler
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Configure WiFi
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    
    // Safely copy WiFi credentials with bounds checking
    strncpy((char*)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    
    strncpy((char*)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

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

static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retrying WiFi connection...");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Connected! IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

static esp_err_t root_handler(httpd_req_t *req)
{
    /*
     * Memory-safe HTML generation for the web interface root page.
     * 
     * This function addresses several critical security issues:
     * 1. Buffer overflow protection with proper size checking
     * 2. Memory allocation error handling 
     * 3. Bounds checking for all string operations
     * 4. Safe HTML content replacement logic
     * 
     * Buffer sizes increased from previous vulnerable implementation:
     * - dynamic_content: 8KB -> 16KB (prevents overflow with max content)
     * - outputs_html: 4KB -> 8KB (handles 5 outputs safely)
     */
    // Generate dynamic content for outputs with sufficient buffer size
    const size_t dynamic_content_size = 16384;  // 16KB for safety
    char* dynamic_content = malloc(dynamic_content_size);
    if (dynamic_content == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for dynamic content");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    // Safely copy base HTML page with bounds checking
    size_t html_page_len = strlen(html_page);
    if (html_page_len >= dynamic_content_size) {
        ESP_LOGE(TAG, "Base HTML page too large for buffer");
        free(dynamic_content);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Internal error");
        return ESP_FAIL;
    }
    strncpy(dynamic_content, html_page, dynamic_content_size - 1);
    dynamic_content[dynamic_content_size - 1] = '\0';
    
    // Replace the outputs div with actual output controls
    const size_t outputs_html_size = 8192;  // 8KB for safety
    char* outputs_html = malloc(outputs_html_size);
    if (outputs_html == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for outputs HTML");
        free(dynamic_content);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    outputs_html[0] = '\0';  // Initialize as empty string
    
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        // Use dynamic allocation with error checking
        const size_t output_block_size = 1024;
        char* output_block = malloc(output_block_size);
        if (output_block == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for output block %d", i);
            free(outputs_html);
            free(dynamic_content);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }
        
        const char* status_class = output_states[i] ? "on" : "off";
        const char* status_text = output_states[i] ? "ON" : "OFF";
        
        char timer_info[200] = "";  // Timer info buffer
        if (output_timers[i].is_active) {
            uint32_t remaining = get_remaining_timer_minutes(i);
            snprintf(timer_info, sizeof(timer_info), 
                "<div class='timer-info'>Timer: %lu minutes remaining</div>", remaining);
        }
        
        // Generate output block with bounds checking
        int written = snprintf(output_block, output_block_size,
            "<div class='output-control'>"
            "<div class='output-header'>Output %d (230V AC)</div>"
            "<div class='control-row'>"
            "<label>Status:</label>"
            "<span class='status %s'>%s</span>"
            "</div>"
            "%s"
            "<div class='control-row'>"
            "<button class='btn-%s' onclick='toggleOutput(%d)'>Turn %s</button>"
            "</div>"
            "<div class='control-row'>"
            "<label>Timer:</label>"
            "<input type='number' id='timer%d' min='1' max='1440' placeholder='Minutes'>"
            "<button class='btn-timer' onclick='setTimer(%d)'>Set Timer</button>"
            "<button class='btn-cancel' onclick='cancelTimer(%d)'>Cancel Timer</button>"
            "</div>"
            "</div>",
            i + 1, status_class, status_text, timer_info,
            output_states[i] ? "off" : "on", i, output_states[i] ? "OFF" : "ON",
            i, i, i);
        
        // Check if output_block was truncated
        if (written >= (int)output_block_size) {
            ESP_LOGW(TAG, "Output block %d was truncated", i);
        }
        
        // Safely concatenate with bounds checking
        size_t current_len = strlen(outputs_html);
        size_t block_len = strlen(output_block);
        if (current_len + block_len >= outputs_html_size) {
            ESP_LOGE(TAG, "Outputs HTML buffer would overflow at output %d", i);
            free(output_block);
            free(outputs_html);
            free(dynamic_content);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Buffer overflow");
            return ESP_FAIL;
        }
        
        strncat(outputs_html, output_block, outputs_html_size - current_len - 1);
        free(output_block);  // Free the dynamically allocated memory
    }
    
    // Replace placeholder with actual content using safer approach
    char* pos = strstr(dynamic_content, "<div id='outputs'>");
    if (pos) {
        char* end = strstr(pos, "</div>");
        if (end) {
            end += 6; // Length of "</div>"
            
            // Calculate sizes for safety
            size_t prefix_len = pos - dynamic_content;
            size_t outputs_len = strlen(outputs_html);
            size_t suffix_len = strlen(end);
            size_t total_needed = prefix_len + outputs_len + suffix_len + 1;
            
            // Check if the result will fit in the buffer
            if (total_needed > dynamic_content_size) {
                ESP_LOGE(TAG, "Dynamic content would overflow buffer (need %zu, have %zu)", 
                         total_needed, dynamic_content_size);
                free(outputs_html);
                free(dynamic_content);
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Content too large");
                return ESP_FAIL;
            }
            
            // Safely move the suffix and insert the outputs HTML
            memmove(pos + outputs_len, end, suffix_len + 1);
            memcpy(pos, outputs_html, outputs_len);
        } else {
            ESP_LOGW(TAG, "Could not find closing </div> tag for outputs");
        }
    } else {
        ESP_LOGW(TAG, "Could not find outputs placeholder in HTML");
    }
    
    // Send response with error checking
    esp_err_t send_result = httpd_resp_send(req, dynamic_content, HTTPD_RESP_USE_STRLEN);
    
    // Clean up allocated memory
    free(outputs_html);
    free(dynamic_content);
    
    if (send_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send HTTP response");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON object");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    cJSON *outputs = cJSON_CreateArray();
    if (outputs == NULL) {
        ESP_LOGE(TAG, "Failed to create JSON array");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        cJSON *output = cJSON_CreateObject();
        if (output == NULL) {
            ESP_LOGE(TAG, "Failed to create JSON object for output %d", i);
            cJSON_Delete(outputs);
            cJSON_Delete(json);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
            return ESP_FAIL;
        }
        
        cJSON_AddNumberToObject(output, "id", i + 1);
        cJSON_AddBoolToObject(output, "state", output_states[i]);
        cJSON_AddBoolToObject(output, "timer_active", output_timers[i].is_active);
        
        if (output_timers[i].is_active) {
            cJSON_AddNumberToObject(output, "remaining_minutes", get_remaining_timer_minutes(i));
        }
        
        cJSON_AddItemToArray(outputs, output);
    }
    
    cJSON_AddItemToObject(json, "outputs", outputs);
    
    const char *json_string = cJSON_Print(json);
    if (json_string == NULL) {
        ESP_LOGE(TAG, "Failed to print JSON");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON serialization failed");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t send_result = httpd_resp_send(req, json_string, strlen(json_string));
    
    free((void*)json_string);
    cJSON_Delete(json);
    
    if (send_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send JSON response");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

static esp_err_t toggle_handler(httpd_req_t *req)
{
    char output_str[10];
    if (httpd_req_get_url_query_str(req, output_str, sizeof(output_str)) != ESP_OK) {
        // Extract from URI path
        const char* uri = req->uri;
        char* output_num_str = strstr(uri, "/output/");
        if (output_num_str) {
            output_num_str += 8; // Skip "/output/"
            int output_num = atoi(output_num_str) - 1; // Convert to 0-based index
            
            if (output_num >= 0 && output_num < NUM_OUTPUTS) {
                web_set_output(output_num, !output_states[output_num]);
                httpd_resp_send(req, "OK", 2);
                return ESP_OK;
            }
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid output number");
    return ESP_FAIL;
}

static esp_err_t timer_handler(httpd_req_t *req)
{
    char content[100];
    size_t content_len = MIN(req->content_len, sizeof(content) - 1);
    
    if (httpd_req_recv(req, content, content_len) <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    content[content_len] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *minutes_json = cJSON_GetObjectItem(json, "minutes");
    if (!cJSON_IsNumber(minutes_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid minutes value");
        return ESP_FAIL;
    }
    
    int minutes = (int)cJSON_GetNumberValue(minutes_json);
    cJSON_Delete(json);
    
    // Extract output number from URI
    const char* uri = req->uri;
    char* output_num_str = strstr(uri, "/output/");
    if (output_num_str) {
        output_num_str += 8; // Skip "/output/"
        int output_num = atoi(output_num_str) - 1; // Convert to 0-based index
        
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
        output_num_str += 8; // Skip "/output/"
        int output_num = atoi(output_num_str) - 1; // Convert to 0-based index
        
        if (output_num >= 0 && output_num < NUM_OUTPUTS) {
            web_cancel_timer(output_num);
            httpd_resp_send(req, "OK", 2);
            return ESP_OK;
        }
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid output number");
    return ESP_FAIL;
}

static esp_err_t wifi_config_page_handler(httpd_req_t *req)
{
    httpd_resp_send(req, wifi_config_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t wifi_scan_handler(httpd_req_t *req)
{
    char json_buffer[2048];
    esp_err_t ret = wifi_config_scan_networks(json_buffer, sizeof(json_buffer));
    
    if (ret == ESP_OK) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, json_buffer, strlen(json_buffer));
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Scan failed");
    }
    
    return ret;
}

static esp_err_t wifi_connect_handler(httpd_req_t *req)
{
    char content[200];
    size_t content_len = MIN(req->content_len, sizeof(content) - 1);
    
    if (httpd_req_recv(req, content, content_len) <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Failed to receive data");
        return ESP_FAIL;
    }
    content[content_len] = '\0';
    
    // Parse JSON
    cJSON *json = cJSON_Parse(content);
    if (json == NULL) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }
    
    cJSON *ssid_json = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_json = cJSON_GetObjectItem(json, "password");
    
    if (!cJSON_IsString(ssid_json) || !cJSON_IsString(password_json)) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid SSID or password");
        return ESP_FAIL;
    }
    
    const char* ssid = cJSON_GetStringValue(ssid_json);
    const char* password = cJSON_GetStringValue(password_json);
    
    // Save credentials
    wifi_credentials_t credentials;
    strncpy(credentials.ssid, ssid, WIFI_SSID_MAX_LEN - 1);
    strncpy(credentials.password, password, WIFI_PASS_MAX_LEN - 1);
    credentials.configured = true;
    
    esp_err_t save_ret = wifi_config_save_credentials(&credentials);
    esp_err_t connect_ret = wifi_config_connect_sta(ssid, password);
    
    // Prepare response
    cJSON *response = cJSON_CreateObject();
    if (response == NULL) {
        ESP_LOGE(TAG, "Failed to create response JSON object");
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }
    
    if (save_ret == ESP_OK && connect_ret == ESP_OK) {
        cJSON_AddBoolToObject(response, "success", true);
        cJSON_AddStringToObject(response, "message", "Connected successfully");
    } else {
        cJSON_AddBoolToObject(response, "success", false);
        cJSON_AddStringToObject(response, "message", "Connection failed");
    }
    
    const char *response_string = cJSON_Print(response);
    if (response_string == NULL) {
        ESP_LOGE(TAG, "Failed to print response JSON");
        cJSON_Delete(response);
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "JSON serialization failed");
        return ESP_FAIL;
    }
    
    httpd_resp_set_type(req, "application/json");
    esp_err_t send_result = httpd_resp_send(req, response_string, strlen(response_string));
    
    free((void*)response_string);
    cJSON_Delete(response);
    cJSON_Delete(json);
    
    if (send_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to send WiFi response");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

esp_err_t start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = WEB_SERVER_PORT;
    config.max_uri_handlers = 10;
    
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
        
        // Toggle output API
        httpd_uri_t toggle_uri = {
            .uri = "/api/output/*/toggle",
            .method = HTTP_POST,
            .handler = toggle_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &toggle_uri);
        
        // Set timer API
        httpd_uri_t timer_uri = {
            .uri = "/api/output/*/timer",
            .method = HTTP_POST,
            .handler = timer_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &timer_uri);
        
        // Cancel timer API
        httpd_uri_t cancel_uri = {
            .uri = "/api/output/*/cancel",
            .method = HTTP_POST,
            .handler = cancel_timer_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &cancel_uri);
        
        // WiFi configuration page
        httpd_uri_t wifi_page_uri = {
            .uri = "/wifi",
            .method = HTTP_GET,
            .handler = wifi_config_page_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_page_uri);
        
        // WiFi scan API
        httpd_uri_t wifi_scan_uri = {
            .uri = "/api/wifi/scan",
            .method = HTTP_GET,
            .handler = wifi_scan_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_scan_uri);
        
        // WiFi connect API
        httpd_uri_t wifi_connect_uri = {
            .uri = "/api/wifi/connect",
            .method = HTTP_POST,
            .handler = wifi_connect_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &wifi_connect_uri);
        
        ESP_LOGI(TAG, "Web server started on port %d", WEB_SERVER_PORT);
        return ESP_OK;
    }
    
    ESP_LOGE(TAG, "Failed to start web server");
    return ESP_FAIL;
}

void web_set_output(uint8_t output_num, bool state)
{
    if (output_num < NUM_OUTPUTS) {
        set_output(output_num, state);
        ESP_LOGI(TAG, "Web: Output %d set to %s", output_num + 1, state ? "ON" : "OFF");
    }
}

void web_set_output_timer(uint8_t output_num, uint32_t duration_minutes)
{
    if (output_num < NUM_OUTPUTS && duration_minutes > 0 && duration_minutes <= MAX_TIMER_DURATION_MINUTES) {
        output_timers[output_num].is_active = true;
        output_timers[output_num].start_time = esp_timer_get_time() / 1000000; // Convert to seconds
        output_timers[output_num].duration_minutes = duration_minutes;
        output_timers[output_num].output_num = output_num;
        
        // Turn on the output when timer is set
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
        uint32_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
        uint32_t elapsed_seconds = current_time - output_timers[output_num].start_time;
        uint32_t total_seconds = output_timers[output_num].duration_minutes * 60;
        
        if (elapsed_seconds >= total_seconds) {
            return 0;
        }
        
        uint32_t remaining_seconds = total_seconds - elapsed_seconds;
        return (remaining_seconds + 59) / 60; // Round up to next minute
    }
    return 0;
}

void process_timers(void)
{
    uint32_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
    
    for (int i = 0; i < NUM_OUTPUTS; i++) {
        if (output_timers[i].is_active) {
            uint32_t elapsed_seconds = current_time - output_timers[i].start_time;
            uint32_t total_seconds = output_timers[i].duration_minutes * 60;
            
            if (elapsed_seconds >= total_seconds) {
                // Timer expired - turn off output
                web_set_output(i, false);
                output_timers[i].is_active = false;
                ESP_LOGI(TAG, "Timer expired: Output %d turned OFF", i + 1);
            }
        }
    }
}
