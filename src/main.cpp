#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <stdlib.h>
#include <stdint.h>
#include <Fonts/FreeSansBold24pt7b.h>    // Large font for main temperatures
#include <Fonts/FreeSansBold18pt7b.h>    // Medium font for smaller temperatures  
#include <Fonts/FreeSans12pt7b.h>        // Regular font for labels
#include <Fonts/FreeSans9pt7b.h>         // Small font for status
#include <driver/twai.h>  // ESP32 TWAI (CAN) driver

// Define pins for the ILI9341 display
#define TFT_CS   5   // Chip Select
#define TFT_RST  4   // Reset
#define TFT_DC   2   // Data/Command
#define TFT_MOSI 9  // SPI Data Out (SDA/MOSI/SDI)
#define TFT_CLK  8  // SPI Clock (SCK)
#define TFT_MISO 7  // SPI Data In (SDO/MISO)

#define STATUS_LED_PIN 2
#define CAN_TX_PIN 17
#define CAN_RX_PIN 16

// F150 specific CAN IDs (discovered from analysis)
#define F150_CLIMATE_CONTROL_ID 0x3D3   // Contains driver and passenger temp settings
#define F150_AMBIENT_DATA_ID 0x3B3      // Contains outside temp data (byte 2)
#define F150_ENGINE_DATA_ID 0x420       // Engine data
#define F150_OBD_RESPONSE_ID 0x7E8      // OBD-II responses

// Create TFT instance
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// Enhanced color palette
#define COLOR_BACKGROUND 0x0841   // Dark blue-grey
#define COLOR_CARD_BG    0x2124   // Lighter grey for cards
#define COLOR_PRIMARY    0x07FF   // Cyan
#define COLOR_TEXT       0xFFFF   // White
#define COLOR_COLD       0x249F   // Light blue
#define COLOR_WARM       0xFD20   // Orange
#define COLOR_HOT        0xF800   // Red
#define COLOR_ACCENT     0x07E0   // Green
#define COLOR_WARNING    0xFFE0   // Yellow

// Temperature variables
float outsideTemp = 0.0;
float hvacTemp1 = 0.0;
float hvacTemp2 = 0.0;
int canMessagesReceived = 0;
uint32_t lastCanMsgId = 0;
bool ledState = false;

// Display update tracking
unsigned long lastUpdateTime = 0;
float lastDisplayedOutside = -999;
float lastDisplayedHvac1 = -999;
float lastDisplayedHvac2 = -999;

// Function prototypes
void initCAN();
// void simulateTemperatures();
void updateDisplay();
void drawStatusBar();
void drawTempCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, float temp, bool isOutside = false);
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, bool filled = false);
uint16_t getTempColor(float temp);
float celsiusToFahrenheit(float celsius);
void printCanMessage(uint32_t id, uint8_t* data, uint8_t length);
void analyzeCanMessage(uint32_t id, uint8_t* data, uint8_t length);
void processTemperatureCanMessages();
bool isPotentialTemperatureMessage(uint32_t id, uint8_t* data, uint8_t length);
void trackMessageChanges(uint32_t id, uint8_t* data, uint8_t length);

// Structure to track CAN message changes
#define MAX_CAN_HISTORY 10
#define MAX_TRACKED_IDS 30

struct CanMessageHistory {
  uint32_t id;
  uint8_t data[8][MAX_CAN_HISTORY]; // Last MAX_CAN_HISTORY values for each byte
  uint8_t length;
  unsigned long timestamp[MAX_CAN_HISTORY];
  uint8_t changeCount[8]; // How many times each byte has changed
  uint8_t currentIndex;
  bool isPotentialTemp;
  float potentialTempValue;
};

// Array to store tracked CAN messages
CanMessageHistory trackedMessages[MAX_TRACKED_IDS];
int trackedMessageCount = 0;

// Function to convert Celsius to Fahrenheit
float celsiusToFahrenheit(float celsius) {
  return (celsius * 9.0 / 5.0) + 32.0;
}

// Function to get temperature color based on Fahrenheit value
uint16_t getTempColor(float tempF) {
  if (tempF < 50) return COLOR_COLD;      // Below 50°F - cold blue
  if (tempF < 77) return COLOR_TEXT;      // 50-77°F - normal white  
  if (tempF < 95) return COLOR_WARM;      // 77-95°F - warm orange
  return COLOR_HOT;                       // Above 95°F - hot red
}

// Function to draw a rounded rectangle (simple version)
void drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, bool filled) {
  if (filled) {
    tft.fillRect(x+2, y, w-4, h, color);
    tft.fillRect(x, y+2, w, h-4, color);
    tft.fillRect(x+1, y+1, w-2, h-2, color);
  } else {
    tft.drawRect(x+2, y, w-4, h, color);
    tft.drawRect(x, y+2, w, h-4, color);
    tft.drawPixel(x+1, y+1, color);
    tft.drawPixel(x+w-2, y+1, color);
    tft.drawPixel(x+1, y+h-2, color);
    tft.drawPixel(x+w-2, y+h-2, color);
  }
}

// Function to draw a temperature card
void drawTempCard(int16_t x, int16_t y, int16_t w, int16_t h, const char* label, float temp, bool isOutside) {
  // Card background
  drawRoundRect(x, y, w, h, COLOR_CARD_BG, true);
  drawRoundRect(x, y, w, h, COLOR_PRIMARY, false);
  
  // Label with FreeFonts
  tft.setTextColor(COLOR_PRIMARY);
  tft.setFont(&FreeSans9pt7b);
  
  // Get text bounds for centering
  int16_t x1, y1;
  uint16_t labelW, labelH;
  tft.getTextBounds(label, 0, 0, &x1, &y1, &labelW, &labelH);
  tft.setCursor(x + (w - labelW) / 2, y + 20);
  tft.print(label);
  
  // Convert to Fahrenheit and get color
  float tempF = celsiusToFahrenheit(temp);
  uint16_t tempColor = getTempColor(tempF);
  tft.setTextColor(tempColor);
  
  // Set font size based on card size
  if (isOutside) {
    tft.setFont(&FreeSansBold24pt7b);
  } else {
    tft.setFont(&FreeSansBold18pt7b);
  }
  
  // Format temperature string
  char tempStr[12];
  sprintf(tempStr, "%.0f F", tempF);
  
  // Get text bounds for centering the temperature
  uint16_t tempW, tempH;
  tft.getTextBounds(tempStr, 0, 0, &x1, &y1, &tempW, &tempH);
  int tempX = x + (w - tempW) / 2;
  int tempY = y + (isOutside ? 65 : 55);
  tft.setCursor(tempX, tempY);
  tft.print(tempStr);
  
  // Reset to default font
  tft.setFont();
  
  // Temperature indicator icon
  int iconY = y + h - 25;
  if (tempF < 60) {
    // Cold indicator - snowflake-like pattern
    tft.fillCircle(x + w/2, iconY, 3, COLOR_COLD);
    tft.drawLine(x + w/2 - 4, iconY, x + w/2 + 4, iconY, COLOR_COLD);
    tft.drawLine(x + w/2, iconY - 4, x + w/2, iconY + 4, COLOR_COLD);
  } else if (tempF > 86) {
    // Hot indicator - sun-like pattern
    tft.fillCircle(x + w/2, iconY, 4, COLOR_WARM);
    for (int i = 0; i < 8; i++) {
      float angle = i * PI / 4;
      int x1 = x + w/2 + cos(angle) * 7;
      int y1 = iconY + sin(angle) * 7;
      int x2 = x + w/2 + cos(angle) * 10;
      int y2 = iconY + sin(angle) * 10;
      tft.drawLine(x1, y1, x2, y2, COLOR_WARM);
    }
  }
}

// Function to draw status bar
void drawStatusBar() {
  // Clear status area
  tft.fillRect(0, 0, 320, 30, COLOR_BACKGROUND);
  
  // Title with FreeFonts
  tft.setTextColor(COLOR_TEXT);
  tft.setFont(&FreeSans12pt7b);
  tft.setCursor(10, 20);
  tft.print("CLIMATE");
  
  // Reset to small font for status
  tft.setFont(&FreeSans9pt7b);
  
  // Connection status indicator
  tft.fillCircle(300, 15, 8, canMessagesReceived > 0 ? COLOR_ACCENT : COLOR_WARNING);
  tft.setTextColor(COLOR_TEXT);
  tft.setCursor(260, 19);
  tft.print("CAN");
  
  // Message counter
  if (canMessagesReceived > 0) {
    tft.setCursor(220, 19);
    tft.print(canMessagesReceived);
  }
  
  // CAN bus hacking hint
//   tft.setTextColor(COLOR_PRIMARY);
//   tft.setCursor(10, 225);
//   tft.print("Serial: CAN messages for bus hacking");
  
  // Reset to default font
  tft.setFont();
}

// Function to print CAN message to serial for debugging
void printCanMessage(uint32_t id, uint8_t* data, uint8_t length) {
  Serial.printf("CAN ID: 0x%03X | Data: ", id);
  for (int i = 0; i < length; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.printf("| Len: %d", length);
  
  // Add temperature analysis hints
  if (isPotentialTemperatureMessage(id, data, length)) {
    Serial.print(" [TEMP?]");
  }
  Serial.println();
  
  // Track and analyze the message
  analyzeCanMessage(id, data, length);
}

// Function to check if a message might contain temperature data
bool isPotentialTemperatureMessage(uint32_t id, uint8_t* data, uint8_t length) {
  // Common patterns for temperature data in vehicles:
  // 1. Values in typical temperature ranges (adjusted for encoding methods)
  // 2. Known HVAC/BCM message IDs for Ford vehicles
  
  // Check for typical Ford F-150 HVAC or BCM message IDs
  if (id == 0x3B3 || id == 0x3D3 || id == 0x410 || id == 0x420 || id == 0x430 ||
      id == 0x7E8 || // OBD-II responses
      (id >= 0x300 && id <= 0x500)) { // Common HVAC range
    
    // Check for temperature-like values in the data
    for (int i = 0; i < length; i++) {
      // Simple check for values in typical ambient temperature range
      // Assumes standard encoding methods
      // Raw byte value 0x00-0xFF could be:
      // - Direct Celsius (-40 to 215°C with 0=0°C)
      // - Direct Celsius (-128 to 127°C with 0x80=0°C)
      // - Direct Fahrenheit (-40 to 215°F)
      // - Scaled value (e.g., 0x00-0xFF = -40°C to 85°C)
      
      uint8_t value = data[i];
      
      // Typical ambient temperature range checks (using various encoding assumptions)
      // Raw value between 0x00-0x7F (-40°C to 87°C) in standard 1:1 encoding
      // Raw value between 0x50-0xA0 (-40°F to 100°F) in standard 1:1 encoding
      // Raw value between 0x30-0x50 (48-80) could be room temp in raw hex value
      if ((value >= 0x20 && value <= 0x80) || // Possible Celsius range
          (value >= 0x50 && value <= 0xA0) || // Possible Fahrenheit range
          (value >= 0x30 && value <= 0x60)) { // Possible direct temperature value
        return true;
      }
    }
  }
  return false;
}

// Function to analyze and track changes in CAN messages
void analyzeCanMessage(uint32_t id, uint8_t* data, uint8_t length) {
  // Find or create message tracking entry
  int idx = -1;
  for (int i = 0; i < trackedMessageCount; i++) {
    if (trackedMessages[i].id == id) {
      idx = i;
      break;
    }
  }
  
  // Create new entry if not found
  if (idx == -1) {
    if (trackedMessageCount < MAX_TRACKED_IDS) {
      idx = trackedMessageCount++;
      trackedMessages[idx].id = id;
      trackedMessages[idx].length = length;
      trackedMessages[idx].currentIndex = 0;
      trackedMessages[idx].isPotentialTemp = false;
      trackedMessages[idx].potentialTempValue = -999;
      
      // Initialize counters
      for (int i = 0; i < 8; i++) {
        trackedMessages[idx].changeCount[i] = 0;
      }
    } else {
      // Too many IDs to track, skip
      return;
    }
  }
  
  // Update message history
  CanMessageHistory* msg = &trackedMessages[idx];
  int currentIdx = msg->currentIndex;
  msg->timestamp[currentIdx] = millis();
  
  // Check which bytes changed compared to previous message
  int prevIdx = (currentIdx + MAX_CAN_HISTORY - 1) % MAX_CAN_HISTORY;
  bool anyChanges = false;
  
  for (int i = 0; i < length; i++) {
    // Store the new data
    msg->data[i][currentIdx] = data[i];
    
    // If not first message, check for changes
    if (msg->timestamp[prevIdx] > 0) {
      if (msg->data[i][currentIdx] != msg->data[i][prevIdx]) {
        msg->changeCount[i]++;
        anyChanges = true;
      }
    }
  }
  
  // Move to next history slot
  msg->currentIndex = (currentIdx + 1) % MAX_CAN_HISTORY;
  
  // Check for temperature-like patterns
  msg->isPotentialTemp = isPotentialTemperatureMessage(id, data, length);
  
  // For potential temperature messages with changes, print extra details
  if (msg->isPotentialTemp && anyChanges) {
    Serial.printf("  [Analysis] ID: 0x%03X - Potential temperature message\n", id);
    Serial.print("  [Byte changes] ");
    
    for (int i = 0; i < length; i++) {
      Serial.printf("%d:%d ", i, msg->changeCount[i]);
    }
    
    // Convert using different temperature encoding assumptions
    for (int i = 0; i < length; i++) {
      uint8_t value = data[i];
      
      // Try different temperature encodings
      float tempC1 = value - 40; // Common automotive: 0=−40°C, 255=215°C
      float tempC2 = value - 128; // Signed byte: 0x80=0°C
      float tempC3 = value; // Direct Celsius
      float tempC4 = (value * 0.5) - 40; // Half-degree precision
      
      if ((tempC1 >= -20 && tempC1 <= 50) || 
          (tempC2 >= -20 && tempC2 <= 50) ||
          (tempC3 >= 0 && tempC3 <= 40) ||
          (tempC4 >= -20 && tempC4 <= 50)) {
        Serial.printf("\n  [Byte %d potential temp] Raw: %d | ", i, value);
        Serial.printf("A: %.1f°C B: %.1f°C C: %.1f°C D: %.1f°C", tempC1, tempC2, tempC3, tempC4);
      }
    }
    Serial.println();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("=== F150 CAN Bus Temperature Monitor ===");
  Serial.println("Serial output shows all CAN messages for reverse engineering");
  Serial.println("Look for patterns when you change HVAC settings!");
  Serial.println("================================================");
  
  pinMode(STATUS_LED_PIN, OUTPUT);
  
  Serial.println("Initializing display...");
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(COLOR_BACKGROUND);
  
  initCAN();
  drawStatusBar();
  
  Serial.println("Setup complete - monitoring CAN bus...");
}

void initCAN() {
  Serial.println("============================================");
  Serial.println("FORD F150 CAN TEMPERATURE DISPLAY");
  Serial.println("============================================");
  Serial.println("DISCOVERED CAN IDs:");
  Serial.println("0x3D3 - Climate Control - Contains driver (byte 0) and passenger (byte 1) temps");
  Serial.println("0x3B3 - Ambient Data - Contains outside temp (byte 2)");
  Serial.println("============================================");
  Serial.println("TRYING 125KBPS BAUD RATE (Changed from 250kbps)");
  Serial.println("TROUBLESHOOTING TIPS:");
  Serial.println("1. Ensure vehicle ignition is fully ON (not just accessory)");
  Serial.println("2. Check OBD-II connector is fully seated");
  Serial.println("3. CAN_H and CAN_L connections may need to be swapped");
  Serial.println("============================================");
  Serial.println();
  
  pinMode(CAN_RX_PIN, INPUT);
  pinMode(CAN_TX_PIN, OUTPUT);
  
  // Initialize TWAI (CAN) driver
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN, TWAI_MODE_NORMAL);
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_125KBITS(); // Trying 125kbps for older Ford
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  
  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("CAN driver installed successfully");
  } else {
    Serial.println("Failed to install CAN driver - check connections");
    return;
  }
  
  // Start TWAI driver
  if (twai_start() == ESP_OK) {
    Serial.println("CAN driver started successfully - waiting for messages");
  } else {
    Serial.println("Failed to start CAN driver - check power to transceiver");
    return;
  }
  
  Serial.println("CAN initialization complete - showing F150 temperatures");
}

// Simulate temperatures and CAN messages for testing
void simulateTemperatures() {
  static float baseOutsideTemp = 21.5;
  static float baseHvac1 = 18.0;
  static float baseHvac2 = 22.0;
  static unsigned long counter = 0;
  counter++;
  
  float timeFactor = (counter % 100) / 100.0;
  outsideTemp = baseOutsideTemp + (timeFactor - 0.5) * 3;
  hvacTemp1 = baseHvac1 + ((counter % 50) / 100.0 - 0.25) * 2;
  hvacTemp2 = baseHvac2 + ((counter % 70) / 100.0 - 0.35) * 2;
  
  // Simulate CAN messages occasionally
  if (counter % 100 == 0) {
    canMessagesReceived++;
    
    // Simulate different CAN messages for demonstration
    uint8_t testData[8];
    switch((counter / 100) % 4) {
      case 0:
        lastCanMsgId = 0x3B3; // BCM message
        testData[0] = 0x42; testData[1] = 0x11; 
        testData[2] = random(0x20, 0x80); // Could be outside temp
        testData[3] = random(0x10, 0x90); 
        testData[4] = 0xFF; testData[5] = 0x00;
        printCanMessage(lastCanMsgId, testData, 6);
        break;
        
      case 1:
        lastCanMsgId = 0x3D3; // HVAC module
        testData[0] = random(0x40, 0x80); // Could be HVAC temp 1
        testData[1] = random(0x30, 0x70); // Could be HVAC temp 2
        testData[2] = 0x12; testData[3] = 0x34;
        printCanMessage(lastCanMsgId, testData, 4);
        break;
        
      case 2:
        lastCanMsgId = 0x420; // Engine data
        testData[0] = 0x7E; testData[1] = random(0x80, 0xC0);
        printCanMessage(lastCanMsgId, testData, 2);
        break;
        
      case 3:
        lastCanMsgId = 0x7E8; // OBD response
        testData[0] = 0x03; testData[1] = 0x41; testData[2] = 0x05;
        testData[3] = random(0x50, 0x90); // Coolant temp
        printCanMessage(lastCanMsgId, testData, 4);
        break;
    }
  }
}

void updateDisplay() {
  static bool firstRun = true;
  static float lastOutsideF = -999;
  static float lastHvac1F = -999; 
  static float lastHvac2F = -999;
  
  // Convert to Fahrenheit for comparison
  float outsideTempF = celsiusToFahrenheit(outsideTemp);
  float hvacTemp1F = celsiusToFahrenheit(hvacTemp1);
  float hvacTemp2F = celsiusToFahrenheit(hvacTemp2);
  
  // Only redraw if values have changed by at least 1 degree F or first run
  bool needsUpdate = firstRun;
  bool outsideChanged = false;
  bool hvac1Changed = false;
  bool hvac2Changed = false;
  
  if (abs(outsideTempF - lastOutsideF) >= 1.0) {
    lastOutsideF = outsideTempF;
    outsideChanged = true;
    needsUpdate = true;
  }
  if (abs(hvacTemp1F - lastHvac1F) >= 1.0) {
    lastHvac1F = hvacTemp1F;
    hvac1Changed = true;
    needsUpdate = true;
  }
  if (abs(hvacTemp2F - lastHvac2F) >= 1.0) {
    lastHvac2F = hvacTemp2F;
    hvac2Changed = true;
    needsUpdate = true;
  }
  
  if (!needsUpdate) {
    return;
  }
  
  if (firstRun) {
    // Draw all cards on first run
    drawTempCard(60, 50, 200, 85, "OUTSIDE", outsideTemp, true);
    drawTempCard(20, 150, 130, 80, "DRIVER ZONE", hvacTemp1, false);
    drawTempCard(170, 150, 130, 80, "PASSENGER", hvacTemp2, false);
    drawStatusBar();
    firstRun = false;
  } else {
    // Only redraw changed cards
    if (outsideChanged) {
      drawTempCard(60, 50, 200, 85, "OUTSIDE", outsideTemp, true);
    }
    if (hvac1Changed) {
      drawTempCard(20, 150, 130, 80, "DRIVER ZONE", hvacTemp1, false);
    }
    if (hvac2Changed) {
      drawTempCard(170, 150, 130, 80, "PASSENGER", hvacTemp2, false);
    }
    
    // Update status bar occasionally
    static int statusCounter = 0;
    if (++statusCounter >= 5) {
      drawStatusBar();
      statusCounter = 0;
    }
  }
  
  lastUpdateTime = millis();
}

// Process real CAN messages from the vehicle
void processCanMessages() {
  static uint32_t totalMessages = 0;
  static uint32_t lastCountTime = 0;
  static bool firstMessageReceived = false;
  
  // Using real CAN hardware with TWAI driver
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    // Count received messages
    totalMessages++;
    firstMessageReceived = true;
    
    // Process received message
    printCanMessage(message.identifier, message.data, message.data_length_code);
    
    // Analyze for temperature data
    analyzeCanMessage(message.identifier, message.data, message.data_length_code);
    
    // Process climate control message (0x3D3) - Driver and Passenger temps
    if (message.identifier == F150_CLIMATE_CONTROL_ID) {
      // Driver temp (byte 0) and Passenger temp (byte 1) using standard automotive encoding
      if (message.data_length_code >= 2) {
        // Standard automotive temperature encoding: value - 40°C
        hvacTemp1 = (message.data[0] - 40.0); // Driver temp
        hvacTemp2 = (message.data[1] - 40.0); // Passenger temp
        
        // Log the found values
        Serial.printf("[FOUND] Climate Control - Driver: %.1f°C, Passenger: %.1f°C\n", 
                      hvacTemp1, hvacTemp2);
      }
    }
    
    // Process ambient data message (0x3B3) - Outside temp data
    else if (message.identifier == F150_AMBIENT_DATA_ID) {
      if (message.data_length_code >= 8) {
        // Get raw byte values for reference
        uint8_t byte0 = message.data[0];
        uint8_t byte1 = message.data[1]; // Using byte 1 for outside temp
        uint8_t byte2 = message.data[2]; // Previous byte used
        
        // FOUND CORRECT ENCODING: Ford F150 uses half-degree precision on byte 1
        // Formula: (value * 0.5) - 40
        outsideTemp = (message.data[1] * 0.5) - 40.0;
        
        // Log the raw values and result
        Serial.printf("[OAT] Raw values: %02X %02X %02X \n", byte0, byte1, byte2);
        Serial.printf("[OAT] Outside Temp: %.1f°C (%.1f°F)\n", 
                     outsideTemp, celsiusToFahrenheit(outsideTemp));
      }
    }
  }
  
  // Print connection status every 3 seconds
  if (millis() - lastCountTime > 3000) {
    lastCountTime = millis();
    
    if (!firstMessageReceived) {
      Serial.println("NO CAN MESSAGES RECEIVED - Try reversing CAN_H and CAN_L connections!");
    } else {
      Serial.printf("CAN Status: Working! Messages received: %d\n", totalMessages);
    }
    
    // Check TWAI status
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
      Serial.printf("Bus state: %s, RX msgs: %d, TX msgs: %d, RX errors: %d, TX errors: %d\n",
                  status.state == TWAI_STATE_RUNNING ? "RUNNING" : "ERROR",
                  status.msgs_to_rx, status.msgs_to_tx,
                  status.rx_error_counter, status.tx_error_counter);
    }
  }
  
  // Simulation disabled - we're now using real CAN messages
}

void loop() {
  // REAL MODE - using actual/simulated CAN messages
  // When connected to actual vehicle, comment out simulateTemperatures()
  // simulateTemperatures();
  
  // Process CAN messages
  processCanMessages();
  
  // Update display regularly
  static unsigned int displayCounter = 0;
  displayCounter++;
  
  if (displayCounter >= 100) {  // Every 1 second
    displayCounter = 0;
    
    // Toggle LED
    ledState = !ledState;
    digitalWrite(STATUS_LED_PIN, ledState);
    
    // Update display
    updateDisplay();
    
    // Debug output - temperatures only (CAN messages printed separately)
    Serial.printf("Current Temps - Outside: %.1f°C (%.0f°F), Driver: %.1f°C (%.0f°F), Passenger: %.1f°C (%.0f°F)\n", 
                  outsideTemp, celsiusToFahrenheit(outsideTemp),
                  hvacTemp1, celsiusToFahrenheit(hvacTemp1), 
                  hvacTemp2, celsiusToFahrenheit(hvacTemp2));
  }
  
  delay(10);
}