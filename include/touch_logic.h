#ifndef TOUCH_LOGIC_H
#define TOUCH_LOGIC_H

// Pure touch mapping, free of Arduino dependencies so it can be unit tested on
// the host. See test/test_touch_logic/.
//
// Calibration is measured, not guessed - see docs/TOUCH_CALIBRATION.md. Getting
// the range wrong is invisible in the middle of the screen and only shows up at
// the edges, which is exactly how the MAXY bug in #9 survived.

struct TouchCal {
  int minX, maxX, minY, maxY;
};

// Measured on this truck's panel by pressing all four edges with the floodtest
// harness. Re-measure after any panel or wiring change - see
// docs/TOUCH_CALIBRATION.md. Do not extrapolate these; that is what #9 was.
#define TOUCH_CAL_MINX 502
#define TOUCH_CAL_MAXX 3731
#define TOUCH_CAL_MINY 749
#define TOUCH_CAL_MAXY 3515

inline TouchCal touchCal() {
  return TouchCal{ TOUCH_CAL_MINX, TOUCH_CAL_MAXX, TOUCH_CAL_MINY, TOUCH_CAL_MAXY };
}

// Both axes run opposite to the display, so the output ranges are reversed.
inline int mapTouchAxis(int raw, int rawMin, int rawMax, int screenMax) {
  if (rawMax == rawMin) return 0;
  long span = (long)rawMax - rawMin;
  long v = screenMax - ((long)(raw - rawMin) * screenMax) / span;
  if (v < 0) v = 0;
  if (v > screenMax) v = screenMax;
  return (int)v;
}

inline int touchScreenX(int rawX, const TouchCal &c) {
  return mapTouchAxis(rawX, c.minX, c.maxX, 319);
}

inline int touchScreenY(int rawY, const TouchCal &c) {
  return mapTouchAxis(rawY, c.minY, c.maxY, 239);
}

// Is a mapped point inside a rectangle, allowing `margin` px of slop?
inline bool touchInRect(int sx, int sy, int x, int y, int w, int h, int margin) {
  return sx >= x - margin && sx < x + w + margin &&
         sy >= y - margin && sy < y + h + margin;
}

#endif // TOUCH_LOGIC_H
