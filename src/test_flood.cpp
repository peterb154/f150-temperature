// Bring-up only: touch calibration capture + manual flood control.
#include <Arduino.h>
#include <SPI.h>
#include <pins.h>
#include <XPT2046_Touchscreen.h>

XPT2046_Touchscreen ts(TOUCH_CS, TOUCH_IRQ);

void setup() {
  pinMode(FLOOD_PIN, OUTPUT);
  digitalWrite(FLOOD_PIN, LOW);
  Serial.begin(115200);
  delay(300);
  // TFT is software-SPI on its own pins, so the hardware SPI bus is free
  SPI.begin(TOUCH_CLK, TOUCH_DO, TOUCH_DIN, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);           // match tft.setRotation(1)
  Serial.println();
  Serial.println("TOUCH CAL - tap and hold each corner of the glass in turn");
  Serial.println("  flood: 1=on 0=off p=pulse");
}

void loop() {
  static unsigned long last = 0;
  if (ts.touched() && millis() - last > 150) {
    TS_Point pt = ts.getPoint();
    Serial.printf("raw x=%4d y=%4d z=%4d\n", pt.x, pt.y, pt.z);
    last = millis();
  }
  if (Serial.available()) {
    switch (Serial.read()) {
      case '1': digitalWrite(FLOOD_PIN, HIGH); Serial.println("ON"); break;
      case '0': digitalWrite(FLOOD_PIN, LOW);  Serial.println("OFF"); break;
      case 'p': digitalWrite(FLOOD_PIN, HIGH); delay(500);
                digitalWrite(FLOOD_PIN, LOW); Serial.println("PULSE"); break;
    }
  }
}
