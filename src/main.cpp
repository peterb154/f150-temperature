#include <Arduino.h>
#include <SPI.h>
#include <pins.h>
#include <colors.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <driver/twai.h>
#include <XPT2046_Touchscreen.h>
#include <Preferences.h>
#include <flood_logic.h>
#include <oat_logic.h>
#include <touch_logic.h>
#include <fuel_logic.h>
#include <stdint.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

// ========== LAYOUT PARAMETERS ==========
// Card dimensions and spacing
#define CARD_WIDTH 95
#define CARD_HEIGHT 100
#define CARD_RADIUS 6
#define CARD_SPACING 10

// OAT card (smaller, top-left)
#define OAT_WIDTH 90
#define OAT_HEIGHT 80
#define OAT_X 10
#define OAT_Y 10

// Bottom row cards
#define BOTTOM_Y 130
#define DRIVER_X 10
#define FAN_X (DRIVER_X + CARD_WIDTH + CARD_SPACING)
#define PASS_X (FAN_X + CARD_WIDTH + CARD_SPACING)

// Fuel percentage - top-centre grid slot (OAT's row, FAN's column)
#define FUEL_X FAN_X
#define FUEL_Y OAT_Y
#define FUEL_W OAT_WIDTH
#define FUEL_H OAT_HEIGHT

// Flood light button - top-right grid slot (OAT's row, PASS's column)
#define FLOOD_X PASS_X
#define FLOOD_Y OAT_Y
#define FLOOD_W OAT_WIDTH
#define FLOOD_H OAT_HEIGHT

// Touch calibration lives in include/touch_logic.h so the firmware and the
// host tests share one set of numbers. See docs/TOUCH_CALIBRATION.md to
// re-measure it.
#define TOUCH_HIT_MARGIN 8   // forgiveness around the button edge
#define TOUCH_DEBOUNCE_MS 250

// "OFF" is three wide glyphs; the 24pt value font overflows a 90px card
#define FLOOD_STATE_FONT &FreeSansBold18pt7b

// High beam arming. Measured stalk toggles ran 1.0-1.7 s, and this truck has no
// separate flash-to-pass, so the delay must outlast a flash or the bar strobes
// oncoming traffic. Turning off is immediate; only turning on waits.
#define HIGH_BEAM_MASK      0x02
#define HIGH_BEAM_ARM_MS    2500
#define HIGH_BEAM_STALE_MS  3000   // no 0x3C3 for this long -> assume beams off

// Font sizes
#define LABEL_FONT &FreeSans9pt7b
#define VALUE_FONT &FreeSansBold24pt7b  // Larger font
#define SMALL_FONT &FreeSansBold12pt7b

// Text positioning offsets
#define LABEL_OFFSET_X 10
#define LABEL_OFFSET_Y 20
#define VALUE_OFFSET_X 15
#define VALUE_OFFSET_Y 80  // Adjusted for larger font
#define OAT_VALUE_OFFSET_Y 65  // OAT card is shorter

// F150 CAN Message IDs based on documentation
#define PID_OAT           0x3C4  // Outside Air Temperature
#define PID_HVAC_TEMP     0x3C8  // HVAC Temperature Settings
#define PID_HVAC_FAN      0x357  // HVAC Fan Speed
#define PID_CONSOLE_LIGHTS 0x3B3 // Console Light Dimming
#define PID_VEHICLE_SPEED 0x423  // Vehicle Speed
#define PID_LIGHTING      0x3C3  // Headlamp / high beam (see F150_HIGH_BEAM.md)
#define PID_FUEL          0x465  // Fuel level, UNCONFIRMED (see fuel_logic.h, #11)

// OAT damping: engine heat skews the sensor high when slow or stopped
#define OAT_MOVING_MPH    20     // Above this speed...
#define OAT_MOVING_MS     30000  // ...for this long, trust the raw OAT
// Dead band on the displayed degree. Larger than one 0.45 F sensor step, so
// quantisation noise near a boundary cannot flip the number (see #7).
#define OAT_HYSTERESIS_F  0.7f

// Console dim scale: night mode uses 1-12, day mode uses 13-18
#define NIGHT_MAX_LEVEL    12
#define MAX_DIM_LEVEL      18
#define MIN_BACKLIGHT_PWM  15  // Never fully dark, even at lowest night dimmer
#define NIGHT_MAX_PWM      170
#define DAY_MIN_PWM        60  // Day gets its own range so the dimmer is visible

// Create TFT instance
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);
// Polled, not IRQ-driven. In IRQ mode the library sets isrWake=false as soon as
// one sample reads below its pressure threshold, and only a new falling edge
// rearms it - so a soft press is sampled once while the finger is still
// settling, discarded, and never looked at again. With no IRQ pin isrWake stays
// true and update() samples on every call.
//
// Note this was NOT the cause of the missed presses in #9 - that was a bad
// calibration range. The latch is a real defect regardless.
XPT2046_Touchscreen ts(TOUCH_CS);

// Flood light state. Mode is what the button selects; lit is what the pin does.
FloodMode floodMode = FLOOD_OFF;
FloodMode prevFloodMode = FLOOD_ARMED;  // mismatch forces the first draw
bool floodLit = false;
bool prevFloodLit = true;

bool highBeamOn = false;
unsigned long highBeamSince = 0;    // when the beams last came on
unsigned long lastLightingMsg = 0;  // for the stale-bus watchdog
Preferences floodPrefs;

// Display data variables
float outsideTemp = 72.0;     // Outside Air Temperature (°F), damped for display
bool oatSeeded = false;       // First OAT reading is shown as-is
unsigned long movingSince = 0; // millis() when speed went above OAT_MOVING_MPH, 0 = slow
int driverTempSet = 72;       // Driver temperature setting (°F)
int passengerTempSet = 70;    // Passenger temperature setting (°F)
int fanSpeed = 3;             // Fan speed level (0-7)
int consoleDimLevel = MAX_DIM_LEVEL; // Console brightness (1-18)
bool dataReceived = false;

// Previous values for dirty flag checking
long shownOAT = OAT_DISPLAY_UNSET;      // degrees we want on screen
long drawnOAT = OAT_DISPLAY_UNSET - 1;  // what is on screen; mismatch forces first draw
int fuelRaw = -1;              // 0x465 byte6, -1 until a frame arrives
int shownFuel = FUEL_UNKNOWN;  // percentage we want on screen
int drawnFuel = -2;            // what is on screen; mismatch forces first draw
int prevDriverTempSet = -1;
int prevPassengerTempSet = -1;
int prevFanSpeed = -1;

// Display update tracking
unsigned long lastDisplayUpdate = 0;
unsigned long lastSimulationUpdate = 0;

// CSV Logging for serial visualization
// Set enableCSVLogging = true to output CSV data compatible with F150 serial visualizer
// This adds minimal performance overhead and is useful for data analysis
bool enableCSVLogging = false;  // Set to true to enable CSV output
unsigned long sessionStartTime = 0;
unsigned long messageCount = 0;
bool simulationMode = false; // Force simulation for testing

// Function prototypes
void setup();
void loop();
void initDisplay();
void initCAN();
void updateDisplay();
void simulateData();
void processCanMessages();
void drawTempCard(int x, int y, int w, int h, const char* label, int temp);
void drawFanCard(int x, int y, int w, int h, int fanLevel);
void drawOATCard(int x, int y, int w, int h, long temp);
float decodeOAT(uint8_t byte6, uint8_t byte7);
float decodeSpeedMph(uint8_t byte0, uint8_t byte1);
void updateOAT(float rawOAT);
int decodeHVACTemp(uint8_t byte0, uint8_t byte1);
int decodeFanSpeed(uint8_t byte3);
int decodeConsoleDim(uint8_t byte3);
void setBacklightBrightness(int level);
void initTouch();
void handleTouch();
void drawFloodCard(int x, int y, int w, int h, FloodMode mode, bool lit);
void drawFuelCard(int x, int y, int w, int h, int percent);
void updateFlood();

// Arduino Setup Function
void setup() {
  // Flood light off before anything else can run
  pinMode(FLOOD_PIN, OUTPUT);
  digitalWrite(FLOOD_PIN, LOW);

  Serial.begin(115200);
  
  if (enableCSVLogging) {
    // CSV header for serial visualization tools
    Serial.println("F150_TEMPERATURE_CSV_START");
    Serial.println("TIMESTAMP_MS,ELAPSED_MS,CAN_ID,LENGTH,BYTE0,BYTE1,BYTE2,BYTE3,BYTE4,BYTE5,BYTE6,BYTE7,EXTENDED,OAT_F,DRIVER_TEMP,PASS_TEMP,FAN_SPEED,CONSOLE_DIM");
    sessionStartTime = millis();
  } else {
    Serial.println("F150 Temperature Display Starting...");
  }
  
  // Restore the saved mode. ARM persists because that is the normal setting;
  // ON deliberately does not, so a key-on never fires the bar by itself.
  floodPrefs.begin("flood", false);
  uint8_t savedMode = floodPrefs.getUChar("mode", FLOOD_OFF);
  floodMode = savedMode == FLOOD_ARMED ? FLOOD_ARMED : FLOOD_OFF;

  initDisplay();
  initTouch();
  Serial.printf("Flood mode restored: saved=%u -> %s\n", savedMode,
                floodMode == FLOOD_ARMED ? "ARM" : "OFF");
  initCAN();
  
  if (enableCSVLogging) {
    Serial.println("# CSV logging enabled - compatible with F150 serial visualizer");
  } else {
    Serial.println("Setup complete - starting main loop");
  }
}

// Arduino Main Loop
void loop() {
  handleTouch();
  processCanMessages();
  updateFlood();
  
  if (simulationMode && millis() - lastSimulationUpdate > 2000) {
    simulateData();
    lastSimulationUpdate = millis();
  }
  
  if (millis() - lastDisplayUpdate > 100) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }
  
  delay(10);
}

// Initialize TFT Display
void initDisplay() {
  pinMode(TFT_LED, OUTPUT);
  setBacklightBrightness(consoleDimLevel);
  
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BACKGROUND);
  
  Serial.println("Display initialized");
}

// Initialize CAN Bus
void initCAN() {
  pinMode(CAN_RX_PIN, INPUT);
  pinMode(CAN_TX_PIN, OUTPUT);
  
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("CAN driver installed successfully");
  } else {
    Serial.println("Failed to install CAN driver - entering simulation mode");
    simulationMode = true;
    return;
  }
  
  if (twai_start() == ESP_OK) {
    Serial.println("CAN driver started successfully");
    // simulationMode remains as initialized (false for normal operation)
  } else {
    Serial.println("Failed to start CAN driver - entering simulation mode");
    simulationMode = true;
  }
}

// Process CAN Messages
void processCanMessages() {
  if (simulationMode) return;
  
  twai_message_t message;
  while (twai_receive(&message, 0) == ESP_OK) {
    dataReceived = true;
    messageCount++;
    
    // Optional CSV logging for serial visualization
    if (enableCSVLogging) {
      unsigned long timestamp = millis();
      unsigned long elapsed = sessionStartTime > 0 ? timestamp - sessionStartTime : 0;
      
      // CSV format: TIMESTAMP,ELAPSED,CAN_ID,LENGTH,BYTE0-7,EXTENDED,OAT,DRIVER,PASS,FAN,DIM
      Serial.print(timestamp);
      Serial.print(",");
      Serial.print(elapsed);
      Serial.print(",0x");
      Serial.print(message.identifier, HEX);
      Serial.print(",");
      Serial.print(message.data_length_code);
      
      // Output data bytes
      for (int i = 0; i < 8; i++) {
        Serial.print(",");
        if (i < message.data_length_code) {
          Serial.print("0x");
          if (message.data[i] < 16) Serial.print("0");
          Serial.print(message.data[i], HEX);
        }
      }
      
      Serial.print(",");
      Serial.print(message.extd ? "true" : "false");
    }
    
    // Decode based on documented F150 CAN messages
    switch (message.identifier) {
      case PID_OAT: // Outside Air Temperature
        if (message.data_length_code >= 8) {
          updateOAT(decodeOAT(message.data[6], message.data[7]));
        }
        break;

      case PID_VEHICLE_SPEED: // Vehicle Speed
        if (message.data_length_code >= 2) {
          float mph = decodeSpeedMph(message.data[0], message.data[1]);
          if (mph <= OAT_MOVING_MPH) {
            movingSince = 0;
          } else if (movingSince == 0) {
            movingSince = millis();
          }
        }
        break;
        
      case PID_HVAC_TEMP: // HVAC Temperature Settings
        if (message.data_length_code >= 4) {
          driverTempSet = decodeHVACTemp(message.data[0], message.data[1]);
          passengerTempSet = decodeHVACTemp(message.data[2], message.data[3]);
          
          // When HVAC is off (driver temp = 0x00,0x00), set fan to 0
          if (driverTempSet == -1) {
            fanSpeed = 0;
          }
        }
        break;
        
      case PID_HVAC_FAN: // Fan Speed
        if (message.data_length_code >= 4) {
          fanSpeed = decodeFanSpeed(message.data[3]);
        }
        break;
        
      case PID_FUEL: // Fuel level - UNCONFIRMED decode, see #11
        if (message.data_length_code >= 7) {
          fuelRaw = message.data[6];
        }
        break;

      case PID_LIGHTING: // Headlamp / high beam state
        if (message.data_length_code >= 1) {
          bool hb = (message.data[0] & HIGH_BEAM_MASK) != 0;
          if (hb && !highBeamOn) highBeamSince = millis();
          highBeamOn = hb;
          lastLightingMsg = millis();
        }
        break;

      case PID_CONSOLE_LIGHTS: // Console Light Dimming
        if (message.data_length_code >= 4) {
          int level = decodeConsoleDim(message.data[3]);
          // Ignore unknown values so the screen never goes dark
          if (level > 0) {
            consoleDimLevel = level;
            setBacklightBrightness(consoleDimLevel);
          }
        }
        break;
    }
    
    // Complete CSV line with decoded values
    if (enableCSVLogging) {
      Serial.print(",");
      Serial.print(outsideTemp, 1);  // OAT with 1 decimal
      Serial.print(",");
      Serial.print(driverTempSet == -1 ? "BLANK" : String(driverTempSet));
      Serial.print(",");
      Serial.print(passengerTempSet == -1 ? "BLANK" : String(passengerTempSet));
      Serial.print(",");
      Serial.print(fanSpeed);
      Serial.print(",");
      Serial.println(consoleDimLevel);
    }
  }
}

// Simulate Data for Bench Testing
void simulateData() {
  static int testIndex = 0;
  static unsigned long lastCycle = 0;
  
  Serial.print("simulateData() called - testIndex: ");
  Serial.print(testIndex);
  Serial.print(", millis: ");
  Serial.print(millis());
  Serial.print(", lastCycle: ");
  Serial.println(lastCycle);
  
  // Initialize lastCycle on first run
  if (lastCycle == 0) {
    lastCycle = millis();
    Serial.println("Initialized lastCycle");
  }
  
  // Test extreme temperature values every 3 seconds
  if (millis() - lastCycle >= 3000) {
    // Cycle through test temperatures: negative, single digit, double digit, triple digit
    float testTemps[] = {-32.0, -5.0, 7.0, 22.0, 45.0, 72.0, 89.0, 104.0, 115.0};
    int numTests = sizeof(testTemps) / sizeof(testTemps[0]);
    
    outsideTemp = testTemps[testIndex % numTests];
    testIndex++;
    lastCycle = millis();
    
    Serial.print(">>> CHANGING OAT to: ");
    Serial.println(outsideTemp);
  }
  
  // Create simulation scenarios including blank values every 6 seconds
  unsigned long scenarioTime = millis() / 6000; // Switch scenarios every 6 seconds
  int scenario = scenarioTime % 5; // 5 different scenarios
  
  static int lastScenario = -1;
  if (scenario != lastScenario) {
    lastScenario = scenario;
    
    switch (scenario) {
      case 0: // Normal operation - both temps active
        driverTempSet = 72;
        passengerTempSet = 70;
        fanSpeed = 4;
        Serial.println("=== SCENARIO 0: Normal HVAC Operation ===");
        break;
        
      case 1: // HVAC Off - driver blank, fan 0
        driverTempSet = -1;  // Blank (HVAC off)
        passengerTempSet = 68;
        fanSpeed = 0;        // Fan off when HVAC off
        Serial.println("=== SCENARIO 1: HVAC System OFF (Driver blank, Fan 0) ===");
        break;
        
      case 2: // Passenger controls disabled
        driverTempSet = 74;
        passengerTempSet = -1; // Blank (passenger controls disabled)
        fanSpeed = 3;
        Serial.println("=== SCENARIO 2: Passenger Controls DISABLED (Pass blank) ===");
        break;
        
      case 3: // Both temps disabled
        driverTempSet = -1;   // Blank
        passengerTempSet = -1; // Blank
        fanSpeed = 0;         // Fan off
        Serial.println("=== SCENARIO 3: All HVAC Controls DISABLED (Both blank) ===");
        break;
        
      case 4: // Mix of extreme temps and blanks
        driverTempSet = 85;   // High temp
        passengerTempSet = -1; // Blank
        fanSpeed = 7;         // Max fan
        Serial.println("=== SCENARIO 4: Mixed State (Driver hot, Pass blank, Fan max) ===");
        break;
    }
    
    Serial.print("Driver: ");
    Serial.print(driverTempSet == -1 ? "BLANK" : String(driverTempSet));
    Serial.print(", Passenger: ");
    Serial.print(passengerTempSet == -1 ? "BLANK" : String(passengerTempSet));
    Serial.print(", Fan: ");
    Serial.println(fanSpeed);
  }
}

// Update Display with Smart Redrawing (only when data changes)
void updateDisplay() {
  // Only redraw cards that have changed data
  // Until a real reading lands, show the card with no number rather than the
  // 72.0 that outsideTemp is initialised to - inventing a temperature that
  // looks exactly like a measured one is worse than showing nothing.
  shownOAT = oatSeeded ? displayedOAT(outsideTemp, shownOAT, OAT_HYSTERESIS_F)
                       : OAT_DISPLAY_UNSET;
  if (shownOAT != drawnOAT) {
    drawnOAT = shownOAT;
    drawOATCard(OAT_X, OAT_Y, OAT_WIDTH, OAT_HEIGHT, shownOAT);
  }
  
  if (driverTempSet != prevDriverTempSet) {
    drawTempCard(DRIVER_X, BOTTOM_Y, CARD_WIDTH, CARD_HEIGHT, "DRIVER", driverTempSet);
    prevDriverTempSet = driverTempSet;
  }
  
  if (fanSpeed != prevFanSpeed) {
    drawFanCard(FAN_X, BOTTOM_Y, CARD_WIDTH, CARD_HEIGHT, fanSpeed);
    prevFanSpeed = fanSpeed;
  }
  
  if (passengerTempSet != prevPassengerTempSet) {
    drawTempCard(PASS_X, BOTTOM_Y, CARD_WIDTH, CARD_HEIGHT, "PASS", passengerTempSet);
    prevPassengerTempSet = passengerTempSet;
  }
  
  // Show simulation mode indicator (only once)
  static bool simModeShown = false;
  if (simulationMode && !simModeShown) {
    tft.setFont(&FreeSans9pt7b);
    tft.setTextColor(COLOR_WARNING);
    
    // Center the text horizontally and vertically on screen
    // Screen is 320x240, center vertically around y=120
    int16_t x1, y1;
    uint16_t w, h;
    tft.getTextBounds("SIMULATION MODE", 0, 0, &x1, &y1, &w, &h);
    int centerX = (320 - w) / 2;
    int centerY = 120;
    
    tft.setCursor(centerX, centerY);
    tft.print("SIMULATION MODE");
    simModeShown = true;
  } else if (!simulationMode && simModeShown) {
    // Clear sim mode text when not in simulation - clear center area
    tft.fillRect(80, 105, 160, 25, COLOR_BACKGROUND);
    simModeShown = false;
  }
  
  shownFuel = fuelPercent(fuelRaw, FUEL_RAW_FULL);
  if (shownFuel != drawnFuel) {
    drawnFuel = shownFuel;
    drawFuelCard(FUEL_X, FUEL_Y, FUEL_W, FUEL_H, shownFuel);
  }

  if (floodMode != prevFloodMode || floodLit != prevFloodLit) {
    drawFloodCard(FLOOD_X, FLOOD_Y, FLOOD_W, FLOOD_H, floodMode, floodLit);
    prevFloodMode = floodMode;
    prevFloodLit = floodLit;
  }
}

// Draw Outside Air Temperature Card
void drawOATCard(int x, int y, int w, int h, long temp) {
  // Card background
  tft.fillRoundRect(x, y, w, h, CARD_RADIUS, COLOR_CARD_BG);
  tft.drawRoundRect(x, y, w, h, CARD_RADIUS, COLOR_PRIMARY);
  
  // Center the label
  tft.setFont(LABEL_FONT);
  tft.setTextColor(COLOR_PRIMARY);
  int16_t x1, y1;
  uint16_t textW, textH;
  tft.getTextBounds("OAT", 0, 0, &x1, &y1, &textW, &textH);
  int centeredX = x + (w - textW) / 2;
  tft.setCursor(centeredX, y + LABEL_OFFSET_Y);
  tft.print("OAT");
  
  // Center the temperature value (larger font, no 'F' suffix).
  // OAT_DISPLAY_UNSET means no reading yet, so leave the value area empty -
  // same convention drawTempCard() uses for a disabled HVAC zone.
  if (temp != OAT_DISPLAY_UNSET) {
    tft.setFont(VALUE_FONT);
    tft.setTextColor(COLOR_TEXT);
    char tempStr[8];
    snprintf(tempStr, sizeof(tempStr), "%ld", temp);
    tft.getTextBounds(tempStr, 0, 0, &x1, &y1, &textW, &textH);
    centeredX = x + (w - textW) / 2;
    tft.setCursor(centeredX, y + OAT_VALUE_OFFSET_Y);
    tft.print(tempStr);
  }
}

// Draw Temperature Setting Card (Driver/Passenger)
void drawTempCard(int x, int y, int w, int h, const char* label, int temp) {
  // Card background
  tft.fillRoundRect(x, y, w, h, CARD_RADIUS, COLOR_CARD_BG);
  tft.drawRoundRect(x, y, w, h, CARD_RADIUS, COLOR_PRIMARY);
  
  // Center the label
  tft.setFont(LABEL_FONT);
  tft.setTextColor(COLOR_PRIMARY);
  int16_t x1, y1;
  uint16_t textW, textH;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &textW, &textH);
  int centeredX = x + (w - textW) / 2;
  tft.setCursor(centeredX, y + LABEL_OFFSET_Y);
  tft.print(label);
  
  // Center the temperature value (larger font) - blank if temp is -1
  tft.setFont(VALUE_FONT);
  tft.setTextColor(COLOR_TEXT);
  
  if (temp == -1) {
    // Display nothing for disabled/off state
    // (Card background already drawn, so nothing to print)
  } else {
    char tempStr[4];
    sprintf(tempStr, "%d", temp);
    tft.getTextBounds(tempStr, 0, 0, &x1, &y1, &textW, &textH);
    centeredX = x + (w - textW) / 2;
    tft.setCursor(centeredX, y + VALUE_OFFSET_Y);
    tft.print(tempStr);
  }
}

// Draw Fan Speed Card (visual bars only)
void drawFanCard(int x, int y, int w, int h, int fanLevel) {
  // Card background
  tft.fillRoundRect(x, y, w, h, CARD_RADIUS, COLOR_CARD_BG);
  tft.drawRoundRect(x, y, w, h, CARD_RADIUS, COLOR_PRIMARY);
  
  // Center the label
  tft.setFont(LABEL_FONT);
  tft.setTextColor(COLOR_PRIMARY);
  int16_t x1, y1;
  uint16_t textW, textH;
  tft.getTextBounds("FAN", 0, 0, &x1, &y1, &textW, &textH);
  int centeredX = x + (w - textW) / 2;
  tft.setCursor(centeredX, y + LABEL_OFFSET_Y);
  tft.print("FAN");
  
  // Draw 7 fan bars (no numeric value) - centered for visual balance
  int barWidth = 8;
  int barSpacing = 11;
  int totalBarWidth = (7 * barWidth) + (6 * (barSpacing - barWidth));
  int startX = x + (w - totalBarWidth) / 2;
  int startY = y + 85;
  
  for (int i = 0; i < 7; i++) {
    int barX = startX + i * barSpacing;
    int barHeight = 6 + (i * 5);  // Taller bars with more height variation
    int barY = startY - barHeight;
    
    if (i < fanLevel) {
      // Active bar
      tft.fillRect(barX, barY, barWidth, barHeight, COLOR_PRIMARY);
    } else {
      // Inactive bar
      tft.fillRect(barX, barY, barWidth, barHeight, COLOR_TEXT);
    }
  }
}

// Decode Outside Air Temperature from CAN bytes 6-7 (see F150_OAT.md)
// Byte 6 is whole °C + 128, top 2 bits of byte 7 are quarter degrees
float decodeOAT(uint8_t byte6, uint8_t byte7) {
  int raw = (byte6 << 2) | (byte7 >> 6);
  float celsius = raw / 4.0 - 128;
  return (celsius * 1.8) + 32.0;
}

// Decode Vehicle Speed from CAN bytes 0-1 (see F150_SPEED.md)
// Big-endian, 0.01 km/h per bit, offset 10000 (= stopped)
float decodeSpeedMph(uint8_t byte0, uint8_t byte1) {
  int raw = (byte0 << 8) | byte1;
  float kph = (raw - 10000) / 100.0;
  return kph / 1.609;
}

// Engine heat only pushes the OAT sensor high. When moving fast enough for
// long enough, show the raw reading. Otherwise only let the display drop.
void updateOAT(float rawOAT) {
  bool atSpeed = movingSince != 0 && millis() - movingSince > OAT_MOVING_MS;
  if (!oatSeeded || atSpeed || rawOAT < outsideTemp) {
    outsideTemp = rawOAT;
    oatSeeded = true;
  }
}

// Decode HVAC Temperature from ASCII decimal bytes
// Returns -1 for blank/disabled state (0x00, 0x00)
int decodeHVACTemp(uint8_t byte0, uint8_t byte1) {
  // Check for disabled/off state (0x00, 0x00)
  if (byte0 == 0x00 && byte1 == 0x00) {
    return -1; // Special value for blank display
  }
  
  // ASCII decimal encoding: tens digit in byte0, ones digit in byte1
  int tens = (byte0 >= '0' && byte0 <= '9') ? (byte0 - '0') : 0;
  int ones = (byte1 >= '0' && byte1 <= '9') ? (byte1 - '0') : 0;
  
  return (tens * 10) + ones;
}

// Decode Fan Speed from byte 3 (7 discrete levels)
int decodeFanSpeed(uint8_t byte3) {
  // Direct byte value to fan speed mapping from F150_HVAC_FAN.md
  // Each level decreases by 0x04 (4 decimal) from 0x1C down to 0x04
  switch(byte3) {
    case 0x1C: return 7;  // HIGH (Maximum airflow)
    case 0x18: return 6;  // High-medium  
    case 0x14: return 5;  // Medium-high
    case 0x10: return 4;  // Medium
    case 0x0C: return 3;  // Medium-low
    case 0x08: return 2;  // Low-medium
    case 0x04: return 1;  // LOW (Minimum airflow)
    default:   return 0;  // OFF or Unknown
  }
}

// Decode Console Dimming Level from byte 3 (see F150_CONSOLE_LIGHTS.md)
// Night mode: 0x01-0x0C, day mode: 0x0D-0x12. Returns 0 for unknown values.
int decodeConsoleDim(uint8_t byte3) {
  if (byte3 >= 1 && byte3 <= MAX_DIM_LEVEL) {
    return byte3;
  }
  return 0;
}

// Set TFT Backlight Brightness based on console dimming level
void setBacklightBrightness(int level) {
  int pwmValue;
  if (level <= NIGHT_MAX_LEVEL) {
    pwmValue = map(level, 1, NIGHT_MAX_LEVEL, MIN_BACKLIGHT_PWM, NIGHT_MAX_PWM);
  } else {
    pwmValue = map(level, NIGHT_MAX_LEVEL + 1, MAX_DIM_LEVEL, DAY_MIN_PWM, 255);
  }
  analogWrite(TFT_LED, pwmValue);
}


// Initialize the XPT2046 touch controller.
// The TFT is software-SPI on its own pins, so the hardware SPI bus is free.
void initTouch() {
  SPI.begin(TOUCH_CLK, TOUCH_DO, TOUCH_DIN, TOUCH_CS);
  ts.begin();
  ts.setRotation(1);  // match tft.setRotation(1)
  Serial.println("Touch initialized");
}

// Toggle the flood light on a press (not a hold or a release)
void handleTouch() {
  static bool wasTouched = false;
  static unsigned long lastToggle = 0;

  bool isTouched = ts.touched();
  if (!isTouched) { wasTouched = false; return; }

  TS_Point p = ts.getPoint();
  TouchCal cal = touchCal();
  int sx = touchScreenX(p.x, cal);
  int sy = touchScreenY(p.y, cal);
  bool hit = touchInRect(sx, sy, FLOOD_X, FLOOD_Y, FLOOD_W, FLOOD_H,
                         TOUCH_HIT_MARGIN);

  if (!wasTouched && hit && millis() - lastToggle > TOUCH_DEBOUNCE_MS) {
    floodMode = (FloodMode)((floodMode + 1) % FLOOD_MODE_COUNT);  // OFF -> ON -> ARM
    floodPrefs.putUChar("mode", (uint8_t)floodMode);
    Serial.printf("FLOOD mode=%s\n", floodMode == FLOOD_OFF ? "OFF"
                                    : floodMode == FLOOD_ON  ? "ON" : "ARM");
    lastToggle = millis();
  }
  wasTouched = true;
}

// Decide whether the bar should actually be lit, and drive the pin.
void updateFlood() {
  unsigned long now = millis();

  // A quiet bus invalidates the cached beam state. Without this the next frame
  // to arrive would find highBeamOn already true, leave highBeamSince stale and
  // relight the bar instantly - skipping the arming delay entirely.
  if (now - lastLightingMsg >= HIGH_BEAM_STALE_MS) highBeamOn = false;

  bool want = shouldLight(floodMode, highBeamOn, now, highBeamSince,
                          lastLightingMsg, HIGH_BEAM_ARM_MS, HIGH_BEAM_STALE_MS);
  if (want != floodLit) {
    floodLit = want;
    digitalWrite(FLOOD_PIN, floodLit ? HIGH : LOW);
    Serial.printf("FLOOD %s\n", floodLit ? "LIT" : "dark");
  }
}

// Draw the flood button. Text is the mode, fill is whether the bar is lit now.
void drawFloodCard(int x, int y, int w, int h, FloodMode mode, bool lit) {
  // Yellow fill only when the bar is actually lit. Unlit, ARM is green so it
  // reads as "ready and waiting" and is distinct from a plain OFF.
  uint16_t bg = lit ? COLOR_WARNING : COLOR_CARD_BG;
  uint16_t fg = lit             ? COLOR_BACKGROUND
              : mode == FLOOD_ARMED ? COLOR_SUCCESS
                                    : COLOR_TEXT;

  tft.fillRoundRect(x, y, w, h, CARD_RADIUS, bg);
  tft.drawRoundRect(x, y, w, h, CARD_RADIUS, lit ? COLOR_TEXT : COLOR_PRIMARY);

  int16_t x1, y1; uint16_t tw, th;
  tft.setFont(LABEL_FONT);
  tft.setTextColor(lit ? COLOR_BACKGROUND : COLOR_PRIMARY);
  tft.getTextBounds("FLOOD", 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + LABEL_OFFSET_Y);
  tft.print("FLOOD");

  const char* state = mode == FLOOD_OFF ? "OFF" : mode == FLOOD_ON ? "ON" : "ARM";
  tft.setFont(FLOOD_STATE_FONT);
  tft.setTextColor(fg);
  tft.getTextBounds(state, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + OAT_VALUE_OFFSET_Y);
  tft.print(state);
}

// Draw the fuel card. Blank until a frame arrives, matching the OAT card.
void drawFuelCard(int x, int y, int w, int h, int percent) {
  tft.fillRoundRect(x, y, w, h, CARD_RADIUS, COLOR_CARD_BG);
  tft.drawRoundRect(x, y, w, h, CARD_RADIUS, COLOR_PRIMARY);

  int16_t x1, y1; uint16_t tw, th;
  tft.setFont(LABEL_FONT);
  tft.setTextColor(COLOR_PRIMARY);
  tft.getTextBounds("FUEL", 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + LABEL_OFFSET_Y);
  tft.print("FUEL");

  if (percent == FUEL_UNKNOWN) return;

  char buf[8];
  snprintf(buf, sizeof(buf), "%d", percent);
  // "100" is three wide glyphs; drop a size so it fits a 90px card
  tft.setFont(percent >= 100 ? FLOOD_STATE_FONT : VALUE_FONT);
  tft.setTextColor(percent <= 15 ? COLOR_WARNING : COLOR_TEXT);
  tft.getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
  tft.setCursor(x + (w - tw) / 2, y + OAT_VALUE_OFFSET_Y);
  tft.print(buf);
}
