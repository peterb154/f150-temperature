# Ford F150 Console Lights Dimming Decoding

## 📍 CAN Message Location

- **PID**: `0x3B3`
- **Console Dimming**: Byte 3
- **Encoding**: Sequential hex values (6 discrete levels)

## 💡 Dimming Level Values

```cpp
// Direct byte value to dimming level mapping
switch(byte3) {
   case 0x12: return "HIGH (Level 6)";
   case 0x11: return "Level 5";
   case 0x10: return "Level 4";
   case 0x0F: return "Level 3";
   case 0x0E: return "Level 2";
   case 0x0D: return "LOW (Level 1)";
   default:   return "OFF or Unknown";
}
```

| Byte3 Value | Dim Level | Brightness | Notes |
|-------------|-----------|------------|-------|
| 0x12        | 6         | HIGH       | Maximum brightness |
| 0x11        | 5         | -          | High-medium |
| 0x10        | 4         | -          | Medium-high |
| 0x0F        | 3         | -          | Medium |
| 0x0E        | 2         | -          | Medium-low |
| 0x0D        | 1         | LOW        | Minimum brightness |
| Other       | 0         | OFF        | Unknown |

## 🔬 Engineering Details

- Encoding: Simple descending sequential pattern
- Resolution: 6 discrete brightness levels
- Pattern: Each level decreases by 1 (0x01)
- Range: 0x0D to 0x12 (13 to 18 decimal)

## 🧮 Alternative Formula

```cpp
// Calculate dimming level from byte value
if (byte3 >= 0x0D && byte3 <= 0x12) {
    dim_level = byte3 - 0x0C;  // Results in 1-6
} else {
    dim_level = 0;  // OFF or invalid
}
```

## 🚗 Implementation Notes

- Works for 2011+ F150 with console dimming controls
- Update rate: Real-time with dimmer button adjustment
- Sequential encoding: each level = previous level - 1
- Covers dashboard, radio, and HVAC display brightness
