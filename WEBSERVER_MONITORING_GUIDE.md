# 🔍 ESP32 Web Server Monitoring & Analysis Guide

## 📊 **Real-time Monitoring Methods**

### 1. **Built-in Serial Monitoring**
```bash
# Monitor ESP32 serial output
idf.py -p /dev/ttyUSB0 monitor

# Filter for web server logs only
idf.py -p /dev/ttyUSB0 monitor | grep "WEB_SERVER"
```

### 2. **HTTP Response Size Analysis**
Your web server now logs:
- Individual chunk sizes
- Total response size
- Response time
- Memory usage
- Success/failure rates

**Expected Output:**
```
I (12345) WEB_SERVER: Sent HTML header: 547 bytes
I (12346) WEB_SERVER: Sent output 1: 312 bytes
I (12347) WEB_SERVER: Sent output 2: 298 bytes
I (12348) WEB_SERVER: Sent output 3: 312 bytes
I (12349) WEB_SERVER: Sent output 4: 298 bytes
I (12350) WEB_SERVER: Sent output 5: 312 bytes
I (12351) WEB_SERVER: Sent footer: 423 bytes
I (12352) WEB_SERVER: === TOTAL HTTP RESPONSE SIZE: 2502 bytes ===
I (12353) WEB_SERVER: === RESPONSE TIME: 157 ms ===
```

---

## 🌐 **Browser-based Analysis**

### 1. **Chrome DevTools**
1. Open Chrome and go to your ESP32 IP address
2. Press **F12** to open DevTools
3. Go to **Network** tab
4. Refresh the page
5. Click on the main request to see:
   - **Response size** (Content-Length)
   - **Transfer time**
   - **Response headers**
   - **Payload breakdown**

### 2. **Firefox Developer Tools**
1. Press **F12** → **Network** tab
2. Look for **Size** and **Time** columns
3. **Response** shows uncompressed size
4. **Transferred** shows actual network transfer

---

## 📡 **Network Analysis Tools**

### 1. **curl Command Line Analysis**
```bash
# Basic response size check
curl -v http://192.168.4.1/ | wc -c

# Detailed timing information
curl -w "Total time: %{time_total}s\nSize: %{size_download} bytes\n" http://192.168.4.1/

# Test with headers
curl -I http://192.168.4.1/
```

### 2. **wget Analysis**
```bash
# Download and show stats
wget --spider --server-response http://192.168.4.1/ 2>&1 | grep -E "(Length|Content-Length)"
```

### 3. **Advanced Network Tools**
```bash
# Using HTTPie (install: pip install httpie)
http --print=HhBb http://192.168.4.1/

# Using ab (Apache Bench) for load testing
ab -n 10 -c 1 http://192.168.4.1/
```

---

## 📊 **Performance Benchmarks**

### **Current Expected Sizes:**
- **HTML Header**: ~547 bytes
- **Per Output Block**: ~300 bytes each (5 × 300 = 1500 bytes)
- **HTML Footer**: ~423 bytes
- **Total Page Size**: ~2,470 bytes (2.4 KB)

### **Response Time Targets:**
- **Local WiFi**: < 200ms
- **Direct AP Connection**: < 100ms
- **Multiple Users**: < 500ms

### **Memory Usage:**
- **Free Heap**: Should stay > 300KB
- **HTTP Server Stack**: ~4KB per connection
- **Buffer Usage**: ~500 bytes per response chunk

---

## 🔧 **Optimization Strategies**

### 1. **Response Size Optimization**
```c
// Compress CSS inline (remove spaces/newlines)
static const char* compressed_css = 
"body{font-family:Arial;margin:20px;background:#f0f0f0}"
".container{max-width:600px;margin:0 auto;background:white;padding:20px}";

// Use shorter variable names in JavaScript
"function t(n){fetch('/api/output/'+n+'/toggle',{method:'POST'})}";
```

### 2. **Chunked Response Benefits**
- ✅ **Lower memory usage** - No large buffers
- ✅ **Faster start** - Browser starts rendering immediately
- ✅ **Better stability** - Less likely to cause crashes
- ✅ **Scalable** - Works with any number of outputs

### 3. **GZIP Compression** (Future Enhancement)
```c
// Add to HTTP server config
httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
// Implement gzip compression for static content
```

---

## 🚨 **Monitoring Alerts**

### **Critical Thresholds:**
- **Response Size > 5KB**: Consider optimization
- **Response Time > 1 second**: Check network/processing
- **Free Heap < 100KB**: Memory leak risk
- **Failed Requests > 5%**: Stability issue

### **Health Check API**
Add this endpoint for monitoring:
```c
// GET /api/health
{
  "status": "ok",
  "free_heap": 387432,
  "uptime_seconds": 3600,
  "requests_served": 127,
  "avg_response_time_ms": 156
}
```

---

## 🛠️ **Debugging Tools**

### 1. **ESP32 Built-in Tools**
```c
// Memory analysis
ESP_LOGI(TAG, "Largest free block: %zu", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
ESP_LOGI(TAG, "Min free heap ever: %zu", esp_get_minimum_free_heap_size());

// Task stack monitoring
UBaseType_t stack_highwater = uxTaskGetStackHighWaterMark(NULL);
ESP_LOGI(TAG, "Stack high water mark: %u", stack_highwater);
```

### 2. **Network Packet Analysis**
```bash
# Capture packets (if you have access to router)
tcpdump -i wlan0 host 192.168.4.1 -w esp32_traffic.pcap

# Analyze with Wireshark
wireshark esp32_traffic.pcap
```

### 3. **Load Testing**
```bash
# Test with multiple concurrent users
ab -n 100 -c 5 http://192.168.4.1/

# Stress test the API endpoints
for i in {1..5}; do
  curl -X POST http://192.168.4.1/api/output/$i/toggle &
done
```

---

## 📈 **Performance Monitoring Dashboard**

You can create a simple monitoring page by adding this endpoint:

```c
// GET /api/monitor
static esp_err_t monitor_handler(httpd_req_t *req)
{
    cJSON *json = cJSON_CreateObject();
    
    // System info
    cJSON_AddNumberToObject(json, "free_heap", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    cJSON_AddNumberToObject(json, "min_free_heap", esp_get_minimum_free_heap_size());
    cJSON_AddNumberToObject(json, "uptime_seconds", esp_timer_get_time() / 1000000);
    
    // Server stats
    cJSON_AddNumberToObject(json, "total_requests", server_stats.total_requests);
    cJSON_AddNumberToObject(json, "successful_requests", server_stats.successful_requests);
    cJSON_AddNumberToObject(json, "failed_requests", server_stats.failed_requests);
    cJSON_AddNumberToObject(json, "total_bytes_sent", server_stats.total_bytes_sent);
    cJSON_AddNumberToObject(json, "max_response_size", server_stats.max_response_size);
    cJSON_AddNumberToObject(json, "avg_response_time_ms", server_stats.avg_response_time_ms);
    
    const char *json_string = cJSON_Print(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_string, strlen(json_string));
    
    free((void*)json_string);
    cJSON_Delete(json);
    return ESP_OK;
}
```

---

## 🎯 **Quick Troubleshooting**

### **Problem: Response too large**
- ✅ Use chunked responses (already implemented)
- ✅ Compress CSS/JavaScript
- ✅ Remove unnecessary HTML

### **Problem: Slow responses**
- ✅ Check WiFi signal strength
- ✅ Monitor free heap memory
- ✅ Reduce concurrent connections

### **Problem: Memory crashes**
- ✅ Monitor heap usage
- ✅ Check for memory leaks
- ✅ Increase LWIP buffer sizes (already done)

---

*This monitoring system will help you track exactly how your web server is performing and identify any issues before they cause crashes.*
