# Ford F150 High Beam Decoding

## 📍 CAN Message Location

- **PID**: `0x3C3`
- **High Beam**: Byte 0, bit 1 (mask `0x02`)
- **Headlamps**: Byte 0, bits 7 / 4 — set together as `0x90` when lights are on

## 🧮 Decoding

```cpp
bool highBeamOn = (byte0 & 0x02) != 0;
```

## 📊 Observed Byte 0 Values (captured 2026-09-26)

| Byte0  | Headlamps | High beam |
|--------|-----------|-----------|
| `0x00` | off       | off       |
| `0x02` | off       | **on**    |
| `0x90` | on        | off       |
| `0x92` | on        | **on**    |

Byte 1 bit 5 is the inverse of the same state (`0x21` → `0x01` on high beam) and is
not used.

## ✅ Verification

Two independent captures, analysed by diffing every byte of every ID:

- **Five toggles**: bit 1 went high at 14.9, 17.1, 18.8, 20.5 and 22.2 s — exactly
  five pulses, no other byte on the bus matched
- **Single flip, held**: bit 1 low until 10.8 s, then high for 72 s continuously

## 🚗 Implementation Notes

- Update rate: ~1 Hz, plus immediately on change (same pattern as `0x3B3`)
- **There is no separate flash-to-pass on this truck** — flashing is cycling the
  high beams, so it is the same bit by construction and cannot be distinguished
- Measured toggle durations were **1.0–1.7 s**, so the flood-arm delay must exceed
  that comfortably; `HIGH_BEAM_ARM_MS` is 2500 ms
- Treat a stale message as high beam **off**: if no `0x3C3` arrives for
  `HIGH_BEAM_STALE_MS`, drop the flood rather than holding the last state
