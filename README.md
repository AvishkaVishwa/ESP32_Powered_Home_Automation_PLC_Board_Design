# ESP32 Automation Board

A comprehensive industrial automation controller based on ESP32 WROOM-32D featuring optocoupler-isolated inputs and solid-state relay outputs with a modern web interface for remote control and timer management.

## 🌟 Features

### Hardware Capabilities
- **5 Optocoupler Inputs**: 12V-24V isolated inputs using PC817/PC827 optocouplers
- **4 SSR Outputs**: 230V AC control via solid-state relays (up to 2A per channel)
- **Galvanic Isolation**: Complete electrical isolation between inputs/outputs and MCU
- **Industrial Grade**: Overcurrent protection and status monitoring
- **ESP32 WROOM-32D**: Dual-core 32-bit processor with WiFi and Bluetooth capabilities

### Software Features
- **Modern Web Interface**: Responsive design with real-time control and status monitoring.
- **Easy Network Access**: Access the web interface using the friendly address **http://autoboard.local** on your local network, thanks to mDNS support.
- **Robust WiFi Connectivity**:
  - **Station (STA) Mode**: Connects to your existing Wi-Fi network for seamless integration.
  - **Access Point (AP) Mode**: Falls back to an access point (`Auto_Board_Setup`) if no saved credentials are found, allowing for easy initial configuration.
  - **Automatic Reconnection**: Persistently tries to connect to the configured Wi-Fi network.
- **Timer Control**: Set automatic timers for each output (up to 24 hours).
- **Real-time Monitoring**: Live I/O status updates every 2 seconds.
- **FreeRTOS Integration**: Multi-tasking with proper resource management for stable, long-term operation.
- **Comprehensive Logging**: Detailed debug information via the serial console for easy troubleshooting.

## 🏗️ Hardware Configuration

### GPIO Pin Mapping

#### 📥 Input Pins (12V-24V, Active LOW)
- **Input 1**: GPIO 4
- **Input 2**: GPIO 5  
- **Input 3**: GPIO 18
- **Input 4**: GPIO 19
- **Input 5**: GPIO 21

#### 📤 Output Pins (230V SSR Control, Active HIGH)
- **Output 1**: GPIO 12
- **Output 2**: GPIO 13
- **Output 3**: GPIO 14
- **Output 5**: GPIO 26

#### 💡 Status LED
- **Status LED**: GPIO 2 (Built-in LED)

## 🌐 Web Interface & Network Access

<img src ="/assets/webinterface.png">

The ESP32 Auto Board hosts a web server that allows you to control and monitor the device from any browser on the same network.

### Accessing the Board
Once connected to your Wi-Fi network, you can access the web interface by navigating to:
**http://autoboard.local**

This address is made possible by mDNS (Multicast DNS), which eliminates the need to know the board's IP address.

### Initial Wi-Fi Setup
If the board has no saved Wi-Fi credentials, it will start in Access Point (AP) mode.
1.  Connect your phone or computer to the Wi-Fi network named `Auto_Board_Setup`.
2.  A captive portal should automatically open. If not, open a browser and navigate to `192.168.4.1`.
3.  Select your home Wi-Fi network, enter the password, and click "Connect".
4.  The board will save the credentials and attempt to connect to your network. The status LED will indicate the connection status.

## 🔌 Custom PCB Design

This project features a professionally designed PCB that integrates all components for a complete industrial automation solution:

### PCB Features
- **Compact Form Factor**: Optimized layout for industrial applications
- **Galvanic Isolation**: Built-in optocouplers and isolation circuits
- **Robust Connectors**: Industrial-grade terminal blocks for field wiring
- **Power Management**: Integrated power supply circuits for stable operation
- **Protection Circuits**: Overcurrent and surge protection for inputs/outputs
- **Status Indicators**: LED indicators for power, WiFi, and I/O status
- **Mounting Options**: Standard DIN rail mounting or panel mount capability

### PCB Specifications
- **Dimensions**: Compact industrial form factor
- **Layers**: Multi-layer design for optimal signal integrity
- **Power Input**: 12V-24V DC industrial power supply
- **Operating Temperature**: Industrial grade components rated for extended temperature range
- **Certifications**: Designed for industrial automation standards

---
### PCB Design Images

#### 🎨 3D Renders & Schematics

<img src ="Àssets/F.zone.png">
<img src ="Àssets/B.Zone.png">

*3D View*
<img src ="Àssets/3D.png">


#### 📸 Manufactured PCB Photos

<img src ="Àssets/Front.jpg">
<img src ="Àssets/back.jpg">

## Testing 

<video src = "https://github.com/user-attachments/assets/6f2359ae-ea8d-4051-988a-b78e77dd2364"></video>

---
## 🚀 Getting Started

### Prerequisites
- ESP-IDF v4.4 or later
- Custom Auto_Board PCB with ESP32 WROOM-32D
- USB cable for programming
- WiFi network for remote access

### Building and Flashing

1. **Clone the repository**:
   ```bash
   git clone <repository-url>
   cd Auto_Board
   ```

2. **Add mDNS as a dependency**:
   The project uses the `mdns` component for easy network discovery. Add it to your project using the ESP-IDF component manager:
   ```bash
   idf.py add-dependency "espressif/mdns^1.2"
   ```

3. **Configure the project**:
   ```bash
   idf.py menuconfig
   ```
   - Ensure the serial port is correctly configured under `Serial Flasher Config`.
   - Save and exit.

4. **Build and Flash**:
   ```bash
   idf.py build flash monitor
   ```

5. **Access the Web Interface**:
   - After flashing, the device will connect to your configured Wi-Fi.
   - Open a browser and go to **http://autoboard.local**.


## 🔧 Configuration

### Hardware Settings
Edit `main/auto_board_config.h` to modify:
- GPIO pin assignments
- Debounce timing
- Timer limits
- Network settings

### Software Configuration
Use `idf.py menuconfig` to configure:
- WiFi settings
- Logging levels
- FreeRTOS parameters
- Memory allocation


## ⚠️ Safety Considerations

- **High Voltage Warning**: SSR outputs control 230V AC - ensure proper electrical safety
- **Isolation**: Always maintain galvanic isolation between high and low voltage circuits
- **Testing**: Test all safety features before connecting to live loads
- **Certification**: Ensure compliance with local electrical codes

## 🤝 Contributing

1. Fork the repository
2. Create a feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

## 📄 License

This project is open source. Please check the license file for details.

## 🆘 Support

For issues, questions, or contributions:
- Check existing documentation in the project
- Review the debugging guides
- Create an issue with detailed information

---
## 🎉 Special Thanks to PCBWay


<div align="center">
  <img src="Àssets/Front.jpg" width="260">   <img src="Àssets/back.jpg" width="260"> 
</div>

<p align="center">
  <a href="https://www.pcbway.com/" target="_blank">
    <img src="https://github.com/AvishkaVishwa/12V-DC-Motor-Speed-Controller-PCB-Design-using-KiCAD/blob/0191b6e02eeb30e176867d2a93ebec854536829a/Images/pcbwaylogo.jpg" alt="PCBWay" width="200"/>
  </a>

</p>

I would like to give a huge shoutout and sincere thanks to **[PCBWay](https://www.pcbway.com/)** for sponsoring the PCB fabrication of this project!

The **build quality, silkscreen clarity, via precision, and copper finish** exceeded expectations. PCBWay’s service was fast, professional, and extremely helpful throughout the production process.

This project wouldn’t have been possible without their generous support. If you’re looking to manufacture professional-grade PCBs at an affordable price, I highly recommend checking them out.

🔗 [Visit PCBWay →](https://www.pcbway.com/)

---

---

> © 2025 Avishka Vishwajith   
