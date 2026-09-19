# Ford F150 Vehicle Speed Decoding

## 📍 CAN Message Location

- **PID**: `0x423`
- **Vehicle Speed**: Bytes 0 & 1 (Big Endian)

## 🧮 Decoding Formula

```cpp
int raw = (byte0 << 8) | byte1;
float kph = (raw - 10000) / 100.0;  // 0.01 km/h per bit, 10000 = stopped
float mph = kph / 1.609;
```

## 📊 Captured Values (2026-09-18)

| Raw     | km/h | mph  | Notes                            |
|---------|------|------|----------------------------------|
| `10000` | 0    | 0    | Stopped                          |
| `13054` | 30.5 | 19.0 | Accelerating                     |
| `15070` | 50.7 | 31.5 | Holding an indicated 30 mph      |

Reads ~5% above the speedometer; close enough for thresholds.

## 🚗 Implementation Notes

- Update rate: ~10 Hz
- Used to decide when the raw OAT reading can be trusted (see `F150_OAT.md`)
