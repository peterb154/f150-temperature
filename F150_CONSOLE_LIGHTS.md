# Ford F150 Console Lights Dimming Decoding

## 📍 CAN Message Location

- **PID**: `0x3B3`
- **Console Dimming**: Byte 3
- **Day/Night Mode**: Byte 0 (`0x00` = day, `0x04` = night)
- **Encoding**: One continuous brightness scale, `0x01`–`0x12` (1–18)

## 💡 Dimming Level Values

The dash light sensor switches between day and night mode. Each mode uses a
different slice of the same scale:

| Mode  | Byte0  | Byte2  | Byte3 (dimmer low → high) | Steps |
|-------|--------|--------|---------------------------|-------|
| Day   | `0x00` | `0x05` | `0x0D` → `0x12`           | 6     |
| Night | `0x04` | `0x04` | `0x01` → `0x0C`           | 12    |

Example frames (captured 2026-09-18):

```text
day   full high: 00 43 05 12 00 00 00 38
day   full low:  00 43 05 0D 00 00 00 38
night full high: 04 83 04 0C 00 00 00 38
night full low:  04 83 04 01 00 00 00 38
```

Byte 1 toggles between `0x40`/`0x43` (day) and `0x80`/`0x83` (night); not used.

## 🧮 Decoding

```cpp
// Byte 3 is the brightness level directly; anything else is unknown
if (byte3 >= 0x01 && byte3 <= 0x12) {
    dim_level = byte3;  // 1-18
}
```

## 🚗 Implementation Notes

- Works for 2011+ F150 with console dimming controls
- Update rate: ~1 Hz, plus immediately on dimmer change
- Unknown values are ignored so the display keeps its last brightness
- Backlight PWM never drops below `MIN_BACKLIGHT_PWM` so the display is always readable
- Early versions only mapped the day range (`0x0D`–`0x12`), which blanked the display in night mode (#1)
