# Ford F150 Temperature Display System

A production-ready Ford F150 temperature display system using ESP32-S3 with CAN bus integration and TFT display. This project provides a clean, automotive-grade interface showing outside air temperature, HVAC driver/passenger temperature settings, and fan speed with pixel-perfect text centering and professional styling.

## 🚗 Project Overview

This system captures and displays real-time Ford F150 climate control data via CAN bus at 125 kbps. It features a modern 4-card layout optimized for readability and includes special handling for HVAC off states, simulation mode for testing, and optional CSV logging for data analysis.

### Key Features

- **Real-time Temperature Display** - OAT, driver/passenger HVAC temps, and fan speed
- **Professional UI** - 4-card layout with pixel-perfect text centering and TrueType fonts
- **HVAC State Handling** - Blank display for disabled/off climate controls
- **Simulation Mode** - Built-in test mode cycling through various temperature scenarios
- **Adaptive Backlight** - PWM brightness control based on console dimming level
- **Optional CSV Logging** - Data capture for serial visualization tools (toggleable)
- **Parameterized Design** - Easy customization of fonts, colors, and layout

## 📁 Project Structure

```text
.
├── src/
│   ├── main.cpp                  # Production temperature display system
│   └── main.cpp.save.logger      # Original CAN logger (backup)
├── include/
│   ├── pins.h                    # Hardware pin assignments
│   └── colors.h                  # TFT display color definitions
├── can_visual_grid.py            # Python visualization tool (legacy)
├── F150_*.md                     # Decoded system documentation
│   ├── F150_CONSOLE_LIGHTS.md
│   ├── F150_HVAC_FAN.md
│   ├── F150_HVAC_TEMP.md
│   └── F150_OAT.md
├── platformio.ini                # ESP32 build configuration
├── pyproject.toml               # Python dependencies (for analysis tools)
└── README.md                    # This documentation
```

## 🔧 Hardware Requirements

### ESP32 Temperature Display Setup

- **ESP32-S3 Development Board** (320KB RAM, 8MB Flash recommended)
- **CAN Transceiver Module** (WCMCU-230 or MCP2551)
- **ILI9341 TFT Display** (320x240, SPI interface)
- **XPT2046 Touch Controller** (optional, for future expansion)

### Pin Configuration

Pin assignments are defined in `include/pins.h`:

```cpp
// CAN Bus pins
#define CAN_TX_PIN 47
#define CAN_RX_PIN 48

// ILI9341 Display pins
#define TFT_CS   45
#define TFT_RST  0
#define TFT_DC   35
#define TFT_MOSI 36
#define TFT_CLK  37
#define TFT_LED  38  // Backlight control
#define TFT_MISO 39

// Touch Controller pins (XPT2046)
#define TOUCH_CLK  40
#define TOUCH_CS   41
#define TOUCH_DIN  42
#define TOUCH_DO   2
#define TOUCH_IRQ  1

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

### ESP32 Temperature Display Firmware

1. **Clone the repository** and navigate to the project directory
2. **Install PlatformIO** (if not already installed)
3. **Build and upload** the temperature display firmware:

    ```bash
    # Upload to ESP32-S3
    pio run --target upload

    # Monitor serial output (optional)
    pio device monitor
    ```

4. **Connect hardware** according to pin configuration above
5. **Power on** - the system will auto-start in simulation mode for testing
6. **Connect to vehicle CAN bus** to see live temperature data

### Main System Components

#### `src/main.cpp` - Temperature Display System

The main firmware provides a production-ready Ford F150 temperature display with:

- **4-Card Layout**: OAT (top-left), Driver Temp, Fan Speed, Passenger Temp (bottom row)
- **CAN Bus Integration**: Decodes Ford F150 PIDs at 125 kbps
  - `0x3C4` - Outside Air Temperature (bytes 6-7)
  - `0x3C8` - HVAC Driver/Passenger Temps (ASCII encoded, bytes 0-3)
  - `0x357` - Fan Speed (7 levels, byte 1)
  - `0x3B3` - Console Dim Level (PWM backlight control, byte 3)
- **Smart Display Logic**: Blank temperatures when HVAC is off/disabled
- **Adaptive Backlight**: PWM brightness based on console dimming
- **Simulation Mode**: Built-in test scenarios for development/demo

#### `include/pins.h` - Hardware Configuration

Centralized pin assignments for ESP32-S3 connections.

#### `include/colors.h` - Display Theme

Defines the professional color scheme using 16-bit RGB565 format:

- **COLOR_BACKGROUND** (`0x0841`) - Dark blue-grey background
- **COLOR_CARD_BG** (`0x2124`) - Lighter grey for temperature cards
- **COLOR_PRIMARY** (`0x07FF`) - Cyan accent color
- **COLOR_TEXT** (`0xFFFF`) - White text
- **COLOR_SUCCESS/WARNING/HOT** - Status and temperature-based colors

Colors can be easily customized by modifying the hex values in this file.

## 📊 System Features

### Temperature Display Layout

**4-Card Professional Interface:**
- **OAT Card** (top-left): Outside Air Temperature with "OAT" label
- **DRIVER Card** (bottom-left): Driver HVAC temperature setting
- **FAN Card** (bottom-center): Visual fan speed bars (7 levels)
- **PASS Card** (bottom-right): Passenger HVAC temperature setting

### Smart Display Features

- **Pixel-Perfect Centering**: All temperature values are precisely centered using TrueType fonts
- **Blank State Handling**: Cards show blank when HVAC is off or controls are disabled
- **Adaptive Backlight**: Display brightness automatically adjusts based on console dimming
- **Professional Styling**: Rounded cards, consistent spacing, automotive-grade appearance

### HVAC State Detection

- **HVAC Off**: When driver temp is 0x00,0x00, display shows blank and fan speed goes to 0
- **Passenger Disabled**: Passenger temp shows blank when controls are disabled
- **Normal Operation**: All temperatures display with clean numeric values (no "F" suffix)
- **Fan Visualization**: Horizontal bars indicate fan speed levels 0-7

### Simulation Mode

**Built-in Test Scenarios** (6-second cycles):
1. **Normal Operation** - All temps active, fan running
2. **HVAC System Off** - Driver blank, fan 0, passenger may be active
3. **Passenger Disabled** - Passenger blank, driver/fan normal
4. **All HVAC Off** - Both temps blank, fan 0
5. **Mixed States** - Various combinations for testing

### Optional CSV Logging

**Data Capture for Analysis:**
- **Toggle Control**: Set `enableCSVLogging = true` in code
- **CSV Format**: Timestamp, CAN ID, raw bytes, decoded temperatures
- **Serial Output**: Compatible with visualization tools
- **Performance**: Minimal overhead when enabled, zero when disabled

## 🔍 CAN Bus Analysis Tools

For reverse-engineering additional Ford F150 systems and analyzing CAN bus data, see the comprehensive analysis tools documentation:

**[CAN_ANALYSIS_README.md](CAN_ANALYSIS_README.md)**

Includes:
- Interactive CAN message visualization (`can_visual_grid.py`)
- Raw CAN logger firmware
- Complete reverse-engineering workflow
- Decoded system documentation
- Temperature calibration tools

## 🛠️ Usage

### Normal Operation

1. **Power On**: System starts automatically and shows simulation data
2. **Connect to Vehicle**: CAN bus connection switches to live data
3. **Display Updates**: Temperature cards update in real-time (10Hz refresh)
4. **Backlight Control**: Display brightness adapts to console dimming

### Configuration Options

**CSV Logging** (in `main.cpp`):

```cpp
bool enableCSVLogging = false;  // Set to true for data capture
```


**Simulation Mode** (forced during development):

```cpp
bool forceSimulation = true;   // Set to false for vehicle connection
```


### Hardware Customization

- **Pin Assignments**: Modify `include/pins.h` for different ESP32 boards
- **Display Colors**: Customize `include/colors.h` for different themes
- **Card Layout**: Adjust constants at top of `main.cpp` for sizing/positioning

## 🛠️ Troubleshooting

### Display Issues

**Blank or corrupted display:**

- Check SPI connections to ILI9341 (see `pins.h`)
- Verify power supply stability (3.3V/5V)
- Ensure correct pin definitions match your hardware


**No temperature updates:**

- Verify vehicle is running (systems sleep when off)
- Check CAN H/L connections and polarity
- Monitor serial output for CAN message reception
- Try simulation mode first (`forceSimulation = true`)


**Backlight not working:**

- Check TFT_LED pin connection (see `pins.h`)
- Verify PWM output is functioning
- Test with manual brightness setting


### Performance Issues

**Slow or flickering display:**

- Check power supply current capacity
- Verify SPI clock speed settings
- Monitor for memory issues in serial output


**CAN bus connection problems:**

- Ensure 125 kbps baud rate
- Verify CAN transceiver (MCU-230 recommended)
- Check termination resistance (120Ω)


### Development Tips

1. **Start with simulation mode** - validates display without vehicle
2. **Monitor serial output** - shows CAN status and decoded values
3. **Use CSV logging** - captures data for offline analysis
4. **Test incrementally** - verify each system (display, CAN, decoding) separately

## 🤝 License

MIT License

---

## Ready for your F150 dashboard! 🚗📊
