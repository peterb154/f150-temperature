#ifndef OAT_LOGIC_H
#define OAT_LOGIC_H

#include <math.h>

// Pure OAT display logic, free of Arduino dependencies so it can be unit
// tested on the host. See test/test_oat_logic/.

// Sentinel for "nothing shown yet"; no real temperature comes near it.
#define OAT_DISPLAY_UNSET (-30000L)

// Pick the whole-degree value to show, with hysteresis.
//
// The sensor steps in 0.25 C (0.45 F), so a reading parked near a half-degree
// boundary crosses it on quantisation noise alone and a plain round() flips the
// display between two numbers indefinitely. The shown value only moves once the
// reading clears it by `hysteresis` degrees, which puts a dead band either side
// of each integer and demands real movement rather than dither.
inline long displayedOAT(float raw, long shown, float hysteresis) {
  if (shown == OAT_DISPLAY_UNSET) return lroundf(raw);
  float delta = raw - (float)shown;
  if (delta < 0.0f) delta = -delta;
  return delta >= hysteresis ? lroundf(raw) : shown;
}

#endif // OAT_LOGIC_H
