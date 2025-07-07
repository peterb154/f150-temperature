#include <Arduino.h>
#include <SPI.h>
#include <pins.h>
#include <colors.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <driver/twai.h>
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

// Create TFT instance
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// Display data variables
float outsideTemp = 72.0;     // Outside Air Temperature (°F)
int driverTempSet = 72;       // Driver temperature setting (°F)
int passengerTempSet = 70;    // Passenger temperature setting (°F)
int fanSpeed = 3;             // Fan speed level (0-7)
int consoleDimLevel = 6;      // Console brightness (0-6)
bool dataReceived = false;

// Previous values for dirty flag checking
float prevOutsideTemp = -999.0;
int prevDriverTempSet = -1;
int prevPassengerTempSet = -1;
int prevFanSpeed = -1;
int prevConsoleDimLevel = -1;
bool needsFullRedraw = true;

// Display update tracking
unsigned long lastDisplayUpdate = 0;
unsigned long lastSimulationUpdate = 0;

// CSV Logging for serial visualization
// Set enableCSVLogging = true to output CSV data compatible with F150 serial visualizer
// This adds minimal performance overhead and is useful for data analysis
bool enableCSVLogging = false;  // Set to true to enable CSV output
unsigned long sessionStartTime = 0;
unsigned long messageCount = 0;
bool simulationMode = true; // Force simulation for testing

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
void drawOATCard(int x, int y, int w, int h, float temp);
float decodeOAT(uint8_t byte6, uint8_t byte7);
int decodeHVACTemp(uint8_t byte0, uint8_t byte1);
int decodeFanSpeed(uint8_t byte3);
int decodeConsoleDim(uint8_t byte3);
void setBacklightBrightness(int level);

// Arduino Setup Function
void setup() {
  Serial.begin(115200);
  
  if (enableCSVLogging) {
    // CSV header for serial visualization tools
    Serial.println("F150_TEMPERATURE_CSV_START");
    Serial.println("TIMESTAMP_MS,ELAPSED_MS,CAN_ID,LENGTH,BYTE0,BYTE1,BYTE2,BYTE3,BYTE4,BYTE5,BYTE6,BYTE7,EXTENDED,OAT_F,DRIVER_TEMP,PASS_TEMP,FAN_SPEED,CONSOLE_DIM");
    sessionStartTime = millis();
  } else {
    Serial.println("F150 Temperature Display Starting...");
  }
  
  initDisplay();
  initCAN();
  
  if (enableCSVLogging) {
    Serial.println("# CSV logging enabled - compatible with F150 serial visualizer");
  } else {
    Serial.println("Setup complete - starting main loop");
  }
}

// Arduino Main Loop
void loop() {
  processCanMessages();
  
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
    Serial.println("CAN driver started - but FORCING simulation mode for testing");
    simulationMode = true; // Force simulation for testing
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
          outsideTemp = decodeOAT(message.data[6], message.data[7]);
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
        
      case PID_CONSOLE_LIGHTS: // Console Light Dimming
        if (message.data_length_code >= 4) {
          consoleDimLevel = decodeConsoleDim(message.data[3]);
          setBacklightBrightness(consoleDimLevel);
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
  // Check if full redraw is needed (first time or simulation mode change)
  if (needsFullRedraw) {
    tft.fillScreen(COLOR_BACKGROUND);
    needsFullRedraw = false;
  }
  
  // Only redraw cards that have changed data
  if (abs(outsideTemp - prevOutsideTemp) > 0.1) {
    drawOATCard(OAT_X, OAT_Y, OAT_WIDTH, OAT_HEIGHT, outsideTemp);
    prevOutsideTemp = outsideTemp;
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
}

// Draw Outside Air Temperature Card
void drawOATCard(int x, int y, int w, int h, float temp) {
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
  
  // Center the temperature value (larger font, no 'F' suffix)
  tft.setFont(VALUE_FONT);
  tft.setTextColor(COLOR_TEXT);
  char tempStr[6];
  sprintf(tempStr, "%.0f", temp);
  tft.getTextBounds(tempStr, 0, 0, &x1, &y1, &textW, &textH);
  centeredX = x + (w - textW) / 2;
  tft.setCursor(centeredX, y + OAT_VALUE_OFFSET_Y);
  tft.print(tempStr);
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

// Decode Outside Air Temperature from CAN bytes 6-7
float decodeOAT(uint8_t byte6, uint8_t byte7) {
  // Combine bytes 6-7 into 16-bit value
  uint16_t rawValue = (byte6 << 8) | byte7;
  
  // Convert to Celsius (subtract offset of 128)
  float celsius = (rawValue / 128.0) - 128.0;
  
  // Convert to Fahrenheit
  return (celsius * 9.0 / 5.0) + 32.0;
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
  // Map raw byte value to fan speed level 0-7
  if (byte3 == 0x00) return 0;
  if (byte3 <= 0x20) return 1;
  if (byte3 <= 0x40) return 2;
  if (byte3 <= 0x60) return 3;
  if (byte3 <= 0x80) return 4;
  if (byte3 <= 0xA0) return 5;
  if (byte3 <= 0xC0) return 6;
  return 7;
}

// Decode Console Dimming Level from byte 3 (6 discrete levels)
int decodeConsoleDim(uint8_t byte3) {
  // Map raw byte value to dimming level 0-6
  if (byte3 == 0x00) return 0;
  if (byte3 <= 0x2A) return 1;
  if (byte3 <= 0x55) return 2;
  if (byte3 <= 0x80) return 3;
  if (byte3 <= 0xAA) return 4;
  if (byte3 <= 0xD5) return 5;
  return 6;
}

// Set TFT Backlight Brightness based on console dimming level
void setBacklightBrightness(int level) {
  // Map dimming level 0-6 to PWM value 0-255
  int pwmValue = map(level, 0, 6, 0, 255);
  analogWrite(TFT_LED, pwmValue);
}
