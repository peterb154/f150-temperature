# Ford F150 Outside Air Temperature Decoding

## 📍 CAN Message Location

- **PID**: `0x3C4`
- **Outside Air Temperature**: Bytes 6 & 7 (Big Endian)
- **Primary Byte**: Byte 6 (whole °C + 128)
- **Secondary Byte**: Byte 7, top 2 bits (quarter degrees C)

## 🌡️ Decoding Formula (Celsius Method)

```cpp
// 10-bit value: byte6 is whole degrees, top 2 bits of byte7 are quarters
int raw = (byte6 << 2) | (byte7 >> 6);
temperature_c = raw / 4.0 - 128;
temperature_f = (temperature_c * 1.8) + 32;
```

Example: `A1 B8` → raw 646 → 33.5°C → 92.3°F

## 🔬 Engineering Details

Encoding: Celsius-based with 128 decimal offset
Resolution: 0.25°C (byte6 + top 2 bits of byte7)
Range: -128°C to +127°C (-198°F to +261°F)
Practical Range: Covers all automotive conditions

## 📊 Tested Temperature Range

Tested: 34°F to 124°F (1°C to 51°C)
Accuracy: ±2-3°F average error

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

## 🔥 Engine Heat Damping

The raw sensor is accurate at speed but heat-soaks when slow or stopped
(captured: 87.3°F → 92.3°F in 10 s after stopping, actual 88°F). The truck
does not broadcast a filtered OAT, so the display damps it:

- **Above 20 mph for 30 s** (speed from `0x423`, see `F150_SPEED.md`): show the raw reading
- **Otherwise**: the display can only drop — engine heat only biases the sensor high
- **Boot**: the first reading is shown as-is (may read high if heat-soaked, like the factory dash)
