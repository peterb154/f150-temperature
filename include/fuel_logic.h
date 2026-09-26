#ifndef FUEL_LOGIC_H
#define FUEL_LOGIC_H

// Pure fuel display logic, free of Arduino dependencies so it can be unit
// tested on the host. See test/test_fuel_logic/.
//
// UNCONFIRMED DECODE - see issue #11.
//
// 0x465 byte6 is believed to be fuel level as gallons x4. With the tank topped
// off it read 116 (29.0 gal) while OBD PID 0x2F reported 80.8%, and the
// resolutions are consistent: PID 0x2F steps 0.392% and dithered 205..207,
// while gallons x4 steps 0.69% and held steady at 116, exactly as a coarser
// encoding of the same signal should.
//
// The truck's own sender under-reads - 80.8% with a full tank - so the display
// scales to the OBSERVED full reading rather than a nominal tank size. That
// makes it read 100% when genuinely full without needing the tank capacity to
// be correct, and leaves one constant to tune.
//
// This is on the display so it can be watched against the dash over several
// tanks. If it tracks, the decode is right. If it diverges, it is not.

#define FUEL_UNKNOWN  (-1)
#define FUEL_RAW_FULL 116   // 0x465 byte6 with the tank topped off, 2026-09-26

// Convert the raw byte to a corrected percentage, or FUEL_UNKNOWN if we have
// not seen a frame yet. Assumes the sender is linear from empty to full; if the
// gauge turns out to have an offset instead of a scale error, the low end will
// read wrong first and that will show up as it drains.
inline int fuelPercent(int raw, int rawFull) {
  if (raw < 0 || rawFull <= 0) return FUEL_UNKNOWN;
  long pct = ((long)raw * 100 + rawFull / 2) / rawFull;  // rounded
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return (int)pct;
}

#endif // FUEL_LOGIC_H
