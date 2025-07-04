# ESP32 Automation Board

A comprehensive automation PCB design for ESP32 WROOM-32D featuring:
- 5 optocoupler isolated inputs (12V-24V)
- 5 solid-state relay (SSR) outputs (230V AC)
- Industrial-grade isolation and protection

## Features

- **Input Protection**: PC817/PC827 optocouplers for galvanic isolation
- **High Voltage Outputs**: SSR control for 230V AC loads up to 25A per channel
- **Safety Features**: Input/output isolation, overcurrent protection, status monitoring
- **Real-time Processing**: FreeRTOS-based task management
- **Debugging**: Serial console with comprehensive logging

## Hardware Specifications

### ESP32 WROOM-32D
- **MCU**: Dual-core Xtensa 32-bit LX6 @ 160MHz
- **Flash**: 4MB
- **RAM**: 520KB SRAM
- **GPIO**: 34 programmable GPIOs
- **Operating Voltage**: 3.3V

### Input Channels (5x)
- **Voltage Range**: 12V - 24V DC
- **Isolation**: 5000V RMS (PC817 optocouplers)
- **Input Impedance**: ~1.2kΩ (with current limiting resistor)
- **Response Time**: <1ms
- **Debounce**: 50ms software debouncing

### Output Channels (5x)
- **Load Voltage**: 230V AC
- **Load Current**: Up to 25A per channel (depends on SSR rating)
- **Control Voltage**: 3.3V DC (ESP32 GPIO)
- **Control Current**: <20mA
- **Switching**: Zero-crossing (recommended SSR type)

## Pin Configuration

### Input Pins (Optocouplers)
```
GPIO 4  -> Input 1 (Optocoupler 1)
GPIO 5  -> Input 2 (Optocoupler 2)
GPIO 18 -> Input 3 (Optocoupler 3)
GPIO 19 -> Input 4 (Optocoupler 4)
GPIO 21 -> Input 5 (Optocoupler 5)
```

### Output Pins (SSR Control)
```
GPIO 12 -> Output 1 (SSR 1)
GPIO 13 -> Output 2 (SSR 2)
GPIO 14 -> Output 3 (SSR 3)
GPIO 27 -> Output 4 (SSR 4)
GPIO 26 -> Output 5 (SSR 5)
```

### Status LED
```
GPIO 2  -> Built-in LED (Status indicator)
```

## Circuit Design

### Optocoupler Input Circuit (per channel)
```
12V-24V ----[R1]----[LED]----[GND]
                      |
                   [PC817]
                      |
               ESP32 GPIO ----[R2]---- 3.3V
```

**Component Values:**
- R1: 1.2kΩ (for 12V) to 2.2kΩ (for 24V) - Input current limiting
- R2: 10kΩ - Pull-up resistor (internal ESP32 pull-up can be used)
- PC817: Standard optocoupler with 5000V isolation

### SSR Output Circuit (per channel)
```
ESP32 GPIO ----[R3]----[SSR+]
                         |
                       [SSR-]----[GND]

230V AC Load ----[SSR Load+]----[SSR Load-]---- 230V AC Neutral
```

**Component Values:**
- R3: 220Ω - Current limiting for SSR LED (optional, most SSRs have internal limiting)
- SSR: Solid State Relay (e.g., FOTEK SSR-25DA, Omron G3MB series)

## Building and Flashing

### Prerequisites
- ESP-IDF v4.4 or later
- Python 3.6 or later
- Serial terminal (e.g., PuTTY, screen, minicom)

### Build Commands
```bash
# Set up ESP-IDF environment
. $HOME/esp/esp-idf/export.sh

# Configure project
idf.py menuconfig

# Build project
idf.py build

# Flash to ESP32
idf.py -p /dev/ttyUSB0 flash monitor
```

### Monitoring
```bash
# View serial output
idf.py -p /dev/ttyUSB0 monitor

# Exit monitor: Ctrl+]
```

## Safety Considerations

### Electrical Safety
⚠️ **WARNING: 230V AC is potentially lethal. Ensure proper safety measures:**

1. **Isolation**: Maintain proper isolation between low and high voltage sections
2. **Enclosure**: Use proper electrical enclosure with safety ratings
3. **Grounding**: Ensure proper grounding of all metal parts
4. **Protection**: Install appropriate fuses and circuit breakers
5. **Testing**: Test with low voltage before connecting 230V loads

## License

This project is open-source and available under the MIT License.

---

**Disclaimer**: This design is provided as-is for educational and development purposes. Proper safety measures and professional review are required for any commercial or high-voltage applications.
