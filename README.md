# Ford F150 CAN Bus Analysis Project

A comprehensive toolkit for capturing, visualizing, and decoding CAN bus data from Ford F150 vehicles (tested on 2011 model). This project combines ESP32-S3 board with a WCMCU-230 CAN transceiver and ILI9341 TFT display with Python visualization tools to reverse-engineer vehicle systems and discover ASCII-encoded control schemes.

## 🚗 Project Overview

This project enables real-time monitoring and analysis of Ford F150 CAN bus communications at 125 kbps. It's particularly effective at discovering Ford's ASCII-encoded control systems, such as HVAC temperature controls that use printable ASCII characters instead of traditional binary encoding.

### Key Features

- **Real-time CAN bus capture** at 125 kbps with ESP32
- **Visual grid interface** showing all PIDs and bytes with change highlighting
- **ASCII/HEX toggle mode** to discover text-based encoding schemes
- **Temperature calibration system** for reverse-engineering sensor formulas
- **Marker-based logging** for correlating changes with user actions
- **Multi-cell analysis** for discovering relationships between bytes

## 📁 Project Structure

```
.
├── src/
│   ├── main.cpp              # Raw CAN logger with TFT display
│   └── main.cpp.save.display # Backup/alternative display version
├── can_visual_grid.py        # Python visualization tool
├── F150_*.md                 # Decoded system documentation
│   ├── F150_CONSOLE_LIGHTS.md
│   ├── F150_HVAC_FAN.md
│   ├── F150_HVAC_TEMP.md
│   └── F150_OAT.md
├── platformio.ini            # ESP32 build configuration
├── pyproject.toml           # Python dependencies
└── README.md                # This file
```

## 🔧 Hardware Requirements

### ESP32 CAN Logger Setup

- **ESP32 Development Board** (ESP32-S3 recommended)
- **CAN Transceiver Module** (WCMCU-230)
- **ILI9341 TFT Display** (320x240, SPI interface)

### Pin Configuration

```cpp
// CAN Bus pins
#define CAN_TX_PIN 17
#define CAN_RX_PIN 16

// ILI9341 Display pins
#define TFT_CS   5
#define TFT_RST  4
#define TFT_DC   2
#define TFT_MOSI 9
#define TFT_CLK  8
#define TFT_MISO 7

// Status LED
#define STATUS_LED_PIN 15
```

### Vehicle Connection

- **CAN High**: Connect to vehicle's CAN H line (used connection to car stereo)
- **CAN Low**: Connect to vehicle's CAN L line (used connection to car stereo)
- **Ground**: Connect to vehicle chassis ground
- **Power**: 12V from vehicle (with appropriate regulation to 3.3V/5V)

⚠️ **Important**: Use proper CAN bus termination and isolation, the MCU-230 has a built-in termination resistor. Always test on a non-critical vehicle first.

## 💻 Software Setup

### ESP32 Firmware

1. **Clone the repository** and navigate to the project directory
2. **Install Uv** <https://uv.pnpm.io/>
3. **Install Dependencies**: `uv sync`
4. **Build and upload** the firmware:

    ```bash
    # Upload to ESP32
    pio run -t upload

    # Monitor serial output
    pio device monitor
    ```

5. **Connect hardware** according to pin configuration above
6. **Start vehicle** and verify CAN connection on display

### Python Visualization Tool

1. **Run the visual grid monitor**:

    ```bash
    # Connect to ESP32 serial port
    uv run can_visual_grid.py /dev/cu.wchusbserial56910252611
    ```

## 🎮 Using the Visual Grid Monitor

### Display Modes

- **HEX Mode**: Shows raw hexadecimal values (default)
- **ASCII Mode**: Shows ASCII characters, highlights printable text
- **Toggle**: Press `A` to switch between modes

### Cell Selection and Analysis

1. **Click cells** to select up to 2 for analysis
2. **Purple cell**: First selection
3. **Cyan cell**: Second selection
4. **Information panel**: Shows ASCII interpretation and temperature calculations

### Color Coding

- **Bright Green**: Recent changes (< 0.5 seconds)
- **Green**: Recent changes (< 1 second)
- **Yellow**: Somewhat recent (< 2 seconds)
- **Light Green**: Printable ASCII characters (ASCII mode only)
- **White**: No recent changes

### Temperature Calibration Mode

1. **Select temperature sensor cells** (e.g., PID 0x3C4, bytes 6&7)
2. **Press `T`** to enter calibration mode
3. **Adjust vehicle controls** and input actual LCD readings when prompted
4. **Press `T` again** to exit and calculate linear regression formula

### Keyboard Controls

- `A` - Toggle ASCII/HEX display mode
- `T` - Toggle temperature calibration mode
- `C` - Clear cell selection
- `↑↓` - Scroll through PIDs
- `ESC` - Exit application

### Marker System

Send `x` via serial console to insert markers for correlating data with specific actions (e.g., "start HVAC adjustment").

## 📊 Decoded Systems

This project has successfully reverse-engineered several F150 systems. Each system is documented in dedicated markdown files with complete decoding formulas:

- [F150_HVAC_TEMP.md](F150_HVAC_TEMP.md)
- [F150_HVAC_FAN.md](F150_HVAC_FAN.md)
- [F150_OAT.md](F150_OAT.md)
- [F150_CONSOLE_LIGHTS.md](F150_CONSOLE_LIGHTS.md)

## 🔧 Development Workflow

### 1. Data Capture Phase

```bash
# Start ESP32 logger
pio run -t upload

# Capture specific system in Python
uv run can_visual_grid.py /dev/ttyUSB0
```

### 2. ASCII Discovery Phase

- Switch to ASCII mode (`A` key)
- Look for green-highlighted printable characters
- Select suspicious byte pairs for analysis
- Ford often uses ASCII digits for human-readable values

### 3. Pattern Analysis

- Select 1-2 cells showing interesting patterns
- Use calibration mode for temperature sensors
- Correlate changes with vehicle control adjustments
- Document findings in new markdown files

### 4. Validation

- Test discovered formulas across temperature ranges
- Verify with multiple vehicle states
- Cross-reference with OBD-II data when available

## 🚀 Production Implementation

> **Note**: This section will be expanded when the production display application is developed.

### Planned Features

- Real-time dashboard display
- Multiple system monitoring
- User-friendly interface
- Historical data logging
- Alert systems for abnormal values

### Target Hardware

- Production-grade ESP32 module
- Automotive-qualified components
- Robust CAN interface
- Enhanced display system

### Software Architecture

- Modular system design
- Configurable PID monitoring
- Over-the-air update capability
- Data export functionality

## 🔍 Technical Details

### CAN Bus Configuration

- **Speed**: 125 kbps (Ford F150 standard)
- **Mode**: Listen-only (non-intrusive)
- **Filter**: Accept all messages for discovery
- **Protocol**: ISO 11898 (CAN 2.0)

### Data Format

The ESP32 outputs CSV format for easy analysis:
```csv
TIMESTAMP_MS,ELAPSED_MS,CAN_ID,LENGTH,BYTE0,BYTE1,BYTE2,BYTE3,BYTE4,BYTE5,BYTE6,BYTE7,EXTENDED
1234567890,5000,0x3C8,8,0x37,0x35,0x37,0x30,0x00,0x00,0x00,0x00,false
```

### Ford's ASCII Encoding Discovery

This project's key insight is Ford's use of ASCII decimal encoding for human-readable diagnostics:

- Temperature setpoints: ASCII digits "60"-"90"
- Easy diagnostic reading without conversion
- Backwards compatible with simple tools
- Self-documenting protocol values

## 🛠️ Troubleshooting

### Common Issues

**No CAN messages received:**

- Verify vehicle is running (some systems sleep when off)
- Check CAN H/L connections and polarity
- Ensure proper termination resistance
- Verify baud rate (125 kbps for F150)

**Display not working:**

- Check SPI connections to ILI9341
- Verify power supply stability
- Ensure proper pin definitions in code

**Python visualization issues:**

- Check serial port permissions on Linux
- Verify correct COM port on Windows

### Debugging Tips

1. **Monitor serial output** for connection status
2. **Use oscilloscope** to verify CAN signal integrity
3. **Check termination** - 120Ω between CAN H and CAN L
4. **Test with known-good CAN tools** first

## 📝 Contributing

### Adding New Systems

1. **Capture data** using the visual grid tool
2. **Document findings** using the template format (see existing `F150_*.md` files)
3. **Include decoding formulas** with clear examples
4. **Test across multiple conditions** for validation

## 📚 References

- [ISO 11898 CAN Standard](https://www.iso.org/standard/63648.html)
- [ESP32 TWAI Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/twai.html)

## 🤝 License

MIT License

---

Happy CAN bus hacking! 🚗💻
