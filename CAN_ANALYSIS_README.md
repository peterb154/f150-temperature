# 🔍 Ford F150 CAN Bus Analysis Tools

This directory contains tools for analyzing and reverse-engineering Ford F150 CAN bus messages. These tools complement the production temperature display system and are useful for discovering new PIDs and decoding protocols.

## 🛠️ Analysis Tools

### `can_visual_grid.py` - Interactive CAN Message Analyzer

A Python-based real-time CAN message visualization tool that displays messages in a grid format for pattern analysis and reverse engineering.

**Features:**
- **Real-time Display**: Live CAN message visualization with color-coded changes
- **Dual View Modes**: HEX (default) and ASCII modes for different analysis needs
- **Cell Selection**: Click up to 2 cells for detailed analysis and correlation
- **Temperature Calibration**: Built-in calibration mode for temperature sensor discovery
- **Pattern Detection**: Color coding highlights recent changes and printable ASCII

### `main.cpp.save.logger` - Raw CAN Logger

ESP32 firmware that captures raw CAN messages and outputs them via serial for analysis tools.

**Output Format:**
```
TIMESTAMP_MS,ELAPSED_MS,CAN_ID,LENGTH,BYTE0,BYTE1,BYTE2,BYTE3,BYTE4,BYTE5,BYTE6,BYTE7,EXTENDED
```

## 🎮 Using the Visual Grid Monitor

### Setup and Launch

1. **Upload Logger Firmware**:
   ```bash
   # Use the logger version (not the display system)
   cp src/main.cpp.save.logger src/main.cpp
   pio run --target upload
   pio device monitor  # Verify CAN messages are being received
   ```

2. **Install Python Dependencies**:
   ```bash
   pip install pygame numpy
   ```

3. **Launch Analysis Tool**:
   ```bash
   python can_visual_grid.py /dev/cu.wchusbserial56910252611
   # Replace with your ESP32's serial port
   ```

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
python can_visual_grid.py /dev/ttyUSB0
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
- Cross-reference with multiple data capture sessions
- Document edge cases and special states

### 5. Integration

- Add new decoders to the production temperature display system
- Update `main.cpp` with new PID handlers
- Test real-time display functionality

## 🚗 Known Ford F150 PIDs

### Temperature Systems
- **0x3C4** - Outside Air Temperature (bytes 6-7, Celsius + 40 offset)
- **0x3C8** - HVAC Driver/Passenger Temps (ASCII encoded, bytes 0-3)

### HVAC Controls  
- **0x357** - Fan Speed (7 levels, byte 1)
- **0x3B3** - Console Dim Level (PWM brightness, byte 3)

### Additional Systems

- Various engine, transmission, and body control PIDs documented in individual markdown files

## 💡 Analysis Tips

1. **Start with ASCII Mode**: Many Ford systems use human-readable ASCII encoding
2. **Look for Patterns**: Temperature values often follow predictable mathematical relationships
3. **Use Multiple Test Points**: Capture data across full range of values for accurate formulas
4. **Cross-Reference**: Compare findings with known automotive protocols and standards
5. **Document Everything**: Include test conditions, vehicle state, and environmental factors

## 🔄 Switching Back to Temperature Display

To return to the production temperature display system:

```bash
# Restore the display firmware
cp src/main.cpp.save.logger src/main.cpp.bak  # backup logger
git checkout src/main.cpp  # restore display system
pio run --target upload
```

The analysis tools and production display system are designed to work together - use the analysis tools to discover new data, then integrate findings into the display system for real-time monitoring.
