# ESP32 Automation Board - Web Interface & Timer Control

## NEW FEATURES ADDED ✨

### 🌐 Web Interface Control
- **Real-time Control**: Turn outputs ON/OFF through web browser
- **Timer Functionality**: Set automatic turn-off timers (1-1440 minutes)
- **Status Monitoring**: Live status updates for all outputs
- **Responsive Design**: Works on desktop, tablet, and mobile devices

### ⏰ Timer Features
- **Flexible Duration**: Set timers from 1 minute to 24 hours (1440 minutes)
- **Auto Turn-off**: Outputs automatically turn OFF when timer expires
- **Multiple Timers**: Each output can have its own independent timer
- **Timer Cancellation**: Cancel active timers at any time
- **Visual Feedback**: See remaining time for active timers

## 🚀 Quick Setup

### 1. Configure WiFi Credentials
Edit the WiFi credentials in `main/web_server.c`:

```c
// WiFi credentials - MODIFY THESE!
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";
```

### 2. Build and Flash
```bash
cd /home/avishka/Auto_Board
idf.py build
idf.py flash monitor
```

### 3. Find ESP32 IP Address
After flashing, monitor the serial output to find the IP address:
```
Connected! IP Address: 192.168.1.100
```

### 4. Access Web Interface
Open your web browser and navigate to:
```
http://192.168.1.100
```

## 🎛️ Web Interface Features

### Output Control Panel
Each output has its own control section with:

- **Current Status**: Shows if output is ON or OFF
- **Toggle Button**: Instantly turn output ON/OFF
- **Timer Input**: Set timer duration (1-1440 minutes)
- **Set Timer Button**: Activate timer and turn output ON
- **Cancel Timer Button**: Stop active timer
- **Timer Status**: Shows remaining time for active timers

### Auto-Refresh
- Page automatically refreshes every 30 seconds
- Manual refresh button available
- Real-time status updates

## 📱 Mobile Responsive Design

The web interface is fully responsive and works great on:
- 📱 Smartphones
- 📟 Tablets  
- 💻 Desktop computers
- 🖥️ Large monitors

## 🔧 API Endpoints

For advanced users, the following REST API endpoints are available:

### GET /api/status
Returns JSON status of all outputs:
```json
{
  "outputs": [
    {
      "id": 1,
      "state": true,
      "timer_active": true,
      "remaining_minutes": 45
    }
  ]
}
```

### POST /api/output/{N}/toggle
Toggle output N (1-5)

### POST /api/output/{N}/timer
Set timer for output N:
```json
{
  "minutes": 30
}
```

### POST /api/output/{N}/cancel
Cancel timer for output N

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

### Timer Settings
Modify in `web_server.h`:
```c
#define MAX_TIMER_DURATION_MINUTES 1440  // Change max duration
```

### Web Server Port
Change port in `web_server.h`:
```c
#define WEB_SERVER_PORT 80  // Default HTTP port
```

### Auto-refresh Interval
Modify JavaScript in `web_server.c`:
```javascript
// Auto-refresh every 30 seconds
setInterval(() => location.reload(), 30000);
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
