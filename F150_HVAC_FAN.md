# Ford F150 HVAC Fan Speed Decoding

## 📍 CAN Message Location

- **PID**: `0x357`
- **Fan Speed**: Byte 3
- **Encoding**: Direct value mapping (7 discrete levels)

## 🌪️ Fan Speed Values

```cpp
// Direct byte value to fan speed mapping
switch(byte3) {
   case 0x1C: return "HIGH (Level 7)";
   case 0x18: return "Level 6";
   case 0x14: return "Level 5";
   case 0x10: return "Level 4";
   case 0x0C: return "Level 3";
   case 0x08: return "Level 2";
   case 0x04: return "LOW (Level 1)";
   default:   return "OFF or Unknown";
}
```

| Byte3 Value | Fan Level | Description |
|-------------|-----------|-------------|
| 0x1C        | 7         | HIGH (Maximum airflow) |
| 0x18        | 6         | High-medium |
| 0x14        | 5         | Medium-high |
| 0x10        | 4         | Medium |
| 0x0C        | 3         | Medium-low |
| 0x08        | 2         | Low-medium |
| 0x04        | 1         | LOW (Minimum airflow) |
| Other       | 0         | OFF or Unknown |


## 🔬 Engineering Details

- Encoding: Simple descending 4-bit pattern
- Resolution: 7 discrete fan speeds
- Pattern: Each level decreases by 0x04 (4 decimal)
- Range: 0x04 to 0x1C (4 to 28 decimal)

## 🧮 Alternative Formula
```cpp// Calculate fan level from byte value
if (byte3 >= 0x04 && byte3 <= 0x1C && (byte3 % 4) == 0) {
    fan_level = (byte3 / 4);  // Results in 1-7
} else {
    fan_level = 0;  // OFF or invalid
}
```

## 🚗 Implementation Notes

Works for 2011+ F150 with manual or automatic HVAC controls
Update rate: Real-time with fan speed adjustment
Simple linear encoding makes this easy to decode
Pattern: Each fan level = previous level + 4