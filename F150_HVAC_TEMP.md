# Ford F150 HVAC Temperature Setpoint Decoding

## 📍 CAN Message Location

- **PID**: `0x3C8`
- **Driver Temperature**: Bytes 0 & 1
- **Passenger Temperature**: Bytes 2 & 3

## 🧮 Decoding Formula (ASCII Method)

```cpp
// Ford encodes position as ASCII decimal digits!
// Example: 0x37, 0x35 = ASCII "75" = position 75

// Convert bytes to ASCII characters
char tens_digit = (char)byte0;  // '6', '7', '8', '9'
char ones_digit = (char)byte1;  // '0' through '9'

// Build position string and convert to integer
char position_str[3] = {tens_digit, ones_digit, '\0'};
int position = atoi(position_str);  // "75" -> 75

// Map position to temperature
if (position == 60) {
    return "LOW";
} else if (position == 90) {
    return "HIGH"; 
} else if (position >= 65 && position <= 84) {
    int temp_f = position - 5;  // 65->60°F, 75->70°F, etc.
    return temp_f;
} else {
    return "UNKNOWN";
}
```

## 🔤 ASCII Encoding Details

- **Byte0 Range**: 0x36-0x39 = ASCII '6'-'9' (tens digit)
- **Byte1 Range**: 0x30-0x39 = ASCII '0'-'9' (ones digit)
- **Position Range**: "60" to "90" (ASCII decimal)
- **Ford's Logic**: Direct ASCII decimal encoding for easy diagnostics

## 📊 Temperature Ranges

- **LOW**: Position 60 (Below 60°F)
- **Normal Range**: 60-83°F (positions 65-84)
- **HIGH**: Position 90 (Above 83°F)
- **Resolution**: 1°F per step

## 🎯 Example Values

| Byte0 | Byte1 | ASCII | Position | Result |
|-------|-------|-------|----------|---------|
| 0x36  | 0x30  | "60"  | 60       | LOW     |
| 0x36  | 0x35  | "65"  | 65       | 60°F    |
| 0x37  | 0x30  | "70"  | 70       | 65°F    |
| 0x37  | 0x35  | "75"  | 75       | 70°F    |
| 0x38  | 0x34  | "84"  | 84       | 79°F    |
| 0x39  | 0x30  | "90"  | 90       | HIGH    |

## 🚗 Implementation Notes

- Both driver and passenger use identical ASCII encoding
- Formula works for 2011+ F150 with dual-zone HVAC
- Update rate: Real-time with HVAC knob adjustments
- Celsius conversion: `temp_c = (temp_f - 32) * 5/9`
- **Key Insight**: Ford uses ASCII digits for human-readable diagnostics
- HVAC OFF: Driver temps will be zero (0x3C8 b0 = 0x00 & b1 = 0x00)
- PASS TEMP OFF: Pass temps will be zero (0x3C8 b2 = 0x00 & b3 = 0x00)
- LOW == 60°F
- HIGH == 85°F, then next click jumps to 90°F
