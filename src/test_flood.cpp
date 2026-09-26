// Bring-up harness. Not built into the display firmware - use `-e floodtest`.
//
//   Touch calibration: press every edge of the glass, then send 'c' to read the
//   observed raw extremes. Those four numbers are TOUCH_CAL_* in
//   include/touch_logic.h. See docs/TOUCH_CALIBRATION.md.
//
//   Flood relay: 1 = on, 0 = off, p = 500 ms pulse. Useful if the touchscreen
//   is ever unavailable, since it is otherwise the only control path.
#include <Arduino.h>
#include <SPI.h>
#include <pins.h>
#include <XPT2046_Touchscreen.h>

XPT2046_Touchscreen ts(TOUCH_CS);  // polled, matching the display firmware

int minX = 4095, maxX = 0, minY = 4095, maxY = 0;
long samples = 0;

void resetCal() {
  minX = 4095; maxX = 0; minY = 4095; maxY = 0; samples = 0;
  Serial.println("calibration reset");
}

void reportCal() {
  if (!samples) { Serial.println("no samples yet"); return; }
  Serial.printf("\n--- %ld samples ---\n", samples);
  Serial.printf("#define TOUCH_CAL_MINX %d\n", minX);
  Serial.printf("#define TOUCH_CAL_MAXX %d\n", maxX);
  Serial.printf("#define TOUCH_CAL_MINY %d\n", minY);
  Serial.printf("#define TOUCH_CAL_MAXY %d\n\n", maxY);
}

void setup() {
  pinMode(FLOOD_PIN, OUTPUT);
  digitalWrite(FLOOD_PIN, LOW);
  Serial.begin(115200);
  delay(300);
  // The TFT is software-SPI on its own pins, so the hardware bus is free
  SPI.begin(TOUCH_CLK, TOUCH_DO, TOUCH_DIN, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);
  Serial.println("\nBRING-UP HARNESS");
  Serial.println("  press every edge of the glass, then 'c' for the constants");
  Serial.println("  c = report   r = reset   1 = flood on   0 = off   p = pulse");
}

void loop() {
  static unsigned long lastLog = 0;
  if (ts.touched()) {
    TS_Point pt = ts.getPoint();
    if (pt.x < minX) minX = pt.x;
    if (pt.x > maxX) maxX = pt.x;
    if (pt.y < minY) minY = pt.y;
    if (pt.y > maxY) maxY = pt.y;
    samples++;
    if (millis() - lastLog > 250) {
      Serial.printf("raw(%4d,%4d) z=%4d   seen x %d..%d  y %d..%d\n",
                    pt.x, pt.y, pt.z, minX, maxX, minY, maxY);
      lastLog = millis();
    }
  }
  if (Serial.available()) {
    switch (Serial.read()) {
      case 'c': reportCal(); break;
      case 'r': resetCal(); break;
      case '1': digitalWrite(FLOOD_PIN, HIGH); Serial.println("flood ON"); break;
      case '0': digitalWrite(FLOOD_PIN, LOW);  Serial.println("flood OFF"); break;
      case 'p': digitalWrite(FLOOD_PIN, HIGH); delay(500);
                digitalWrite(FLOOD_PIN, LOW);  Serial.println("flood PULSE"); break;
    }
  }
  delay(10);
}
