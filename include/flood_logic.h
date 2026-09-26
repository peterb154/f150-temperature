#ifndef FLOOD_LOGIC_H
#define FLOOD_LOGIC_H

// Pure flood-light decision logic, deliberately free of Arduino dependencies so
// it can be unit tested on the host. See test/test_flood_logic/.

enum FloodMode {
  FLOOD_OFF,
  FLOOD_ON,
  FLOOD_ARMED,
  FLOOD_MODE_COUNT  // terminator, used to cycle the button
};

// Should the bar be lit right now?
//
// All times are millis(); unsigned subtraction makes rollover a non-issue.
// A lighting frame older than staleMs means the bus went quiet, which is
// treated as beams-off rather than trusting a stale reading.
//
// ARMED requires the beams to have been on continuously for armMs. That delay
// is the only thing separating a deliberate hold from a flash-to-pass, since
// this truck reports both on the same bit.
inline bool shouldLight(FloodMode mode,
                        bool highBeamOn,
                        unsigned long now,
                        unsigned long highBeamSince,
                        unsigned long lastLightingMsg,
                        unsigned long armMs,
                        unsigned long staleMs) {
  if (mode == FLOOD_ON) return true;
  if (mode != FLOOD_ARMED) return false;
  if (now - lastLightingMsg >= staleMs) return false;
  return highBeamOn && (now - highBeamSince >= armMs);
}

#endif // FLOOD_LOGIC_H
