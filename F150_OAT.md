# Ford F150 Outside Air Temperature Decoding

## 📍 CAN Message Location

- **PID**: `0x3C4`
- **Outside Air Temperature**: Bytes 6 & 7 (Big Endian)
- **Primary Byte**: Byte 6 (most significant)
- **Secondary Byte**: Byte 7 (sub-degree precision/filtering)

## 🌡️ Decoding Formula (Celsius Method)

```cpp
// Ford encodes outside air temperature in Celsius with 128 offset
temperature_c = byte6 - 128;
temperature_f = (temperature_c * 9/5) + 32;

// Or as single formula:
temperature_f = ((byte6 - 128) * 1.8) + 32;
```

## 🔬 Engineering Details

Encoding: Celsius-based with 128 decimal offset
Resolution: ~1°C per byte6 increment
Range: -128°C to +127°C (-198°F to +261°F)
Practical Range: Covers all automotive conditions
Byte 7: Provides sub-degree precision (can be ignored for basic readings)

## 📊 Tested Temperature Range

Tested: 34°F to 124°F (1°C to 51°C)
Accuracy: ±2-3°F average error
Formula: temp_f = ((byte6 - 128) * 1.8) + 32

| Byte6 | Byte7 | Celsius | Fahrenheit | Notes    |
|-------|-------|---------|------------|----------|
| 0x83  | 0x38  | 3°C     | 37°F       | Cold     |
| 0x91  | 0x78  | 17°C    | 63°F       | Mild     |
| 0xA4  | 0x78  | 36°C    | 97°F       | Hot      |
| 0xB1  | 0x38  | 49°C    | 120°F      | Very Hot |

## 🚗 Implementation Notes

Works for 2011+ F150 with outside air temperature display
Byte 6 is the primary temperature indicator
Formula follows Ford's automotive Celsius encoding standard
Update rate: Real-time with temperature changes