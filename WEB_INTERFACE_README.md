# ESP32 Automation Board - Interactive Web Interface & Timer Control

## ✨ NEW INTERACTIVE FEATURES ✨

### � Modern Interactive Web Interface
- **🎯 Real-time Control**: Instant toggle control with visual feedback
- **🌈 Gradient Design**: Beautiful modern UI with animations and hover effects
- **📱 Fully Responsive**: Perfect on mobile, tablet, and desktop
- **⚡ Live Status Updates**: Real-time status monitoring every 2 seconds
- **🔄 Smart Loading**: Loading indicators and connection status
- **🎭 Smooth Animations**: Hover effects, button ripples, and status pulses
- **📍 GPIO Pin Display**: Shows exact GPIO pins for each input/output

### 🏗️ Hardware GPIO Configuration

#### 📥 Input Pins (12V-24V Optocoupler)
- **Input 1**: GPIO 4 (12V-24V input, Active LOW)
- **Input 2**: GPIO 5 (12V-24V input, Active LOW)  
- **Input 3**: GPIO 18 (12V-24V input, Active LOW)
- **Input 4**: GPIO 19 (12V-24V input, Active LOW)
- **Input 5**: GPIO 21 (12V-24V input, Active LOW)

#### 📤 Output Pins (230V AC SSR Control)
- **Output 1**: GPIO 12 (230V SSR control, Active HIGH)
- **Output 2**: GPIO 13 (230V SSR control, Active HIGH)
- **Output 3**: GPIO 14 (230V SSR control, Active HIGH)
- **Output 4**: GPIO 27 (230V SSR control, Active HIGH)
- **Output 5**: GPIO 26 (230V SSR control, Active HIGH)

#### 💡 Status Indicator
- **Status LED**: GPIO 2 (Built-in LED, shows system status)

### ⏰ Enhanced Timer Features
- **⏱️ Visual Timer Display**: Active timers show with blinking animation
- **🎯 Precise Control**: Set timers from 1 minute to 24 hours (1440 minutes)
- **🔄 Auto Turn-off**: Outputs automatically turn OFF when timer expires
- **🎪 Multiple Timers**: Each output has independent timer control
- **❌ Quick Cancel**: Cancel active timers with one click
- **📊 Real-time Countdown**: See remaining time updated every 2 seconds

## 🚀 Quick Setup

### 1. Configure WiFi Credentials
Edit the WiFi credentials in `main/web_server.c`:

```c
// WiFi credentials - MODIFY THESE!
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

Or use the web interface WiFi settings page after initial setup.

### 2. Build and Flash
```bash
cd /home/avishka/Auto_Board
idf.py build
idf.py flash monitor
```

### 3. Find ESP32 IP Address
After flashing, monitor the serial output to find the IP address:
```
I (12345) WIFI_CONFIG: Connected! IP Address: 192.168.1.100
I (12346) WEB_SERVER: Web server started on port 80
I (12347) AUTO_BOARD: Access the automation interface at: http://192.168.1.100
```

### 4. Access Interactive Web Interface
Open your web browser and navigate to:
```
http://192.168.1.100
```

You'll see a beautiful, modern interface with:
- 🎨 Gradient background design
- 📊 Real-time system status corner  
- 🎛️ Interactive output control cards
- 📍 GPIO pin reference display
- ⚡ Live animations and feedback

## 🎛️ Interactive Web Interface Features

### 🎨 Modern Control Panel
Each output has its own beautifully designed control card featuring:

- **🏷️ Output Identification**: Clear output number and GPIO pin display
- **📍 GPIO Pin Reference**: Shows both input and output GPIO pins
- **🚦 Animated Status Indicator**: 
  - 🟢 **ACTIVE** (Green with pulsing animation)
  - 🔴 **INACTIVE** (Red status display)
- **🎯 Smart Toggle Buttons**:
  - 🟢 **Turn ON** button when output is OFF
  - 🔴 **Turn OFF** button when output is ON
  - ✨ Hover effects and click animations
- **⏰ Timer Controls**:
  - Number input with validation (1-1440 minutes)
  - **⏰ Set Timer** button with loading feedback
  - **❌ Cancel** button for active timers
- **📊 Timer Status Display**: Shows remaining time with blinking animation

### 🌐 Real-time System Corner
Fixed position status panel showing:
- **⏰ Current Time**: Live clock updating every second
- **⚡ System Uptime**: Days, hours, minutes format
- **🌐 WiFi Status**: Connection indicator with status dot
- **💾 Memory Usage**: Free heap memory display
- **⏱️ Active Timers**: Count of currently running timers

### 🎭 Interactive Elements
- **🔄 Loading Animations**: Spinner overlay during operations
- **🌊 Hover Effects**: Buttons lift and glow on hover
- **💫 Ripple Effects**: Click animations on buttons
- **🚨 Connection Status**: Bottom-left indicator shows API health
- **📱 Mobile Optimized**: Touch-friendly responsive design

## 📱 Mobile Responsive Design

The web interface is fully responsive and works great on:
- 📱 Smartphones
- 📟 Tablets  
- 💻 Desktop computers
- 🖥️ Large monitors

## 🔧 API Endpoints

For advanced users and automation, the following REST API endpoints are available:

### GET /api/status
Returns comprehensive JSON status of all outputs and system information:
```json
{
  "outputs": [
    {
      "id": 1,
      "state": true,
      "timer_active": true,
      "timer_remaining": 45,
      "timer_duration": 60
    }
  ],
  "system": {
    "free_heap": 189432,
    "uptime_seconds": 3600,
    "total_requests": 150,
    "wifi_connected": true,
    "active_timers": 2
  },
  "status": "ok",
  "timestamp": 1625097600
}
```

### POST /api/output/{N}/toggle
Toggle output N (1-5) - Returns "OK" on success

### POST /api/output/{N}/timer
Set timer for output N with JSON payload:
```json
{
  "minutes": 30
}
```

### POST /api/output/{N}/cancel
Cancel active timer for output N - Returns "OK" on success

### WiFi Configuration Endpoints
- **GET /settings** - WiFi configuration page
- **POST /api/wifi/connect** - Connect to new WiFi network
- **POST /api/wifi/reset** - Reset WiFi settings

## ⚠️ Safety Features

### Timer Safety
- **Maximum Duration**: 24 hours (1440 minutes) maximum timer
- **Automatic Shutoff**: Outputs turn OFF when timer expires
- **Fail-safe**: If system resets, timers are cleared (outputs turn OFF)
- **Visual Confirmation**: Clear indication of timer status

### Network Security
- **Local Network Only**: Web interface only accessible on local WiFi
- **No Authentication**: Simple interface for internal use
- **State Validation**: All commands validated before execution

## 🏗️ Technical Implementation

### File Structure
```
main/
├── main.c              # Main application and initialization
├── auto_board.c        # GPIO and hardware functions
├── auto_board_tasks.c  # FreeRTOS tasks (including timer task)
├── web_server.c        # Web server and API implementation
├── auto_board.h        # Hardware definitions
├── auto_board_config.h # Configuration constants
└── web_server.h        # Web server definitions
```

### Task Architecture
- **Input Task**: Monitors optocoupler inputs (Priority: 10)
- **Output Control Task**: Manages manual output control (Priority: 8)
- **Timer Processing Task**: Handles web timers (Priority: 7)
- **Status LED Task**: System status indication (Priority: 5)
- **Web Server**: HTTP request handling (Built-in ESP-IDF)

### Memory Usage
- **Web Server**: ~32KB RAM
- **Timer Storage**: ~80 bytes per output
- **JSON Processing**: ~2KB temporary buffers
- **Total Additional**: ~40KB RAM usage

## 🛠️ Customization Options

### 🎨 Visual Customization
Modify in `web_server.c` within the CSS styles:
```css
/* Change primary colors */
.btn-on { background: linear-gradient(135deg, #your-color, #your-darker-color); }

/* Modify animations */
@keyframes pulse-on { /* Your animation */ }

/* Update responsive breakpoints */
@media (max-width: 768px) { /* Your mobile styles */ }
```

### ⏰ Timer Settings
Modify in `web_server.h`:
```c
#define MAX_TIMER_DURATION_MINUTES 1440  // Change max duration (default: 24 hours)
```

### 🌐 Web Server Configuration
Change settings in `web_server.h`:
```c
#define WEB_SERVER_PORT 80  // Default HTTP port
```

### 🔄 Update Intervals
Modify JavaScript in `web_server.c`:
```javascript
// Status updates every 2 seconds
setInterval(updateStatus, 2000);

// Real-time corner updates every second  
setInterval(updateRealTimeCorner, 1000);
```

### 🏗️ GPIO Pin Reassignment
Modify pin assignments in `auto_board.h`:
```c
// Input pins (12V-24V optocoupler)
#define INPUT_1_GPIO    GPIO_NUM_4   // Change as needed
#define INPUT_2_GPIO    GPIO_NUM_5
// ... etc

// Output pins (230V SSR control)  
#define OUTPUT_1_GPIO   GPIO_NUM_12  // Change as needed
#define OUTPUT_2_GPIO   GPIO_NUM_13
// ... etc
```

## 🐛 Troubleshooting

### WiFi Connection Issues
1. **Check Credentials**: Verify SSID and password in `web_server.c`
2. **Signal Strength**: Ensure ESP32 is within WiFi range
3. **Network Band**: Ensure router supports 2.4GHz (ESP32 doesn't support 5GHz)
4. **Monitor Output**: Check serial monitor for connection status

### Web Interface Not Loading
1. **Check IP Address**: Verify IP from serial monitor
2. **Firewall**: Ensure no firewall blocking connection
3. **Browser Cache**: Try hard refresh (Ctrl+F5)
4. **Network**: Ensure device is on same WiFi network

### Timer Not Working
1. **Check Logs**: Monitor serial output for timer messages
2. **Time Validation**: Ensure timer value is 1-1440 minutes
3. **System Time**: ESP32 uses internal timer (resets on reboot)
4. **Memory**: Verify sufficient RAM available

## 📊 Status Monitoring

### Serial Monitor Output
```
I (12345) AUTO_BOARD: === AUTOMATION BOARD STATUS ===
INPUTS:  IN1:INA IN2:ACT IN3:INA IN4:INA IN5:INA 
OUTPUTS: OUT1:ON OUT2:OFF OUT3:ON OUT4:OFF OUT5:OFF 
I (12345) WEB_SERVER: Timer set for Output 1 - 30 minutes
I (12345) WEB_SERVER: Timer expired: Output 3 turned OFF
```

### LED Status Indicators
- **Blinking**: System running normally
- **Solid ON**: System error or stuck
- **OFF**: Power/system failure

## 🔮 Future Enhancements

Potential features for future versions:
- 🔐 User authentication and security
- 📊 Usage statistics and logging
- 📅 Scheduled timer presets
- 📧 Email/SMS notifications
- 🌡️ Temperature monitoring
- 📱 Mobile app companion
- ☁️ Cloud integration
- 🔄 Backup/restore settings

## 📞 Support

For technical support:
- 📋 Check serial monitor output
- 🔍 Review this documentation
- 🐛 Create GitHub issue with logs
- 💬 ESP32 community forums

---

**⚡ Your ESP32 Automation Board now has web control and timer functionality! ⚡**
