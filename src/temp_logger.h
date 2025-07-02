#pragma once
#include <Arduino.h>
#include <driver/twai.h>

// Structure to track temperature candidate messages
struct TempCandidate {
  uint32_t id;
  uint8_t lastData[8];
  uint8_t currentData[8];
  uint8_t length;
  bool hasChanged;
  unsigned long lastChangeTime;
  int changeCount[8];  // Count of changes per byte
  float tempValues[8]; // Potential temperature values
};

// Maximum number of temperature candidates to track
#define MAX_TEMP_CANDIDATES 20

// Utility functions for temperature detection
void logDetailedCanMessage(twai_message_t message);
void trackTemperatureCandidate(twai_message_t message);
void displayTemperatureCandidates();
float decodePotentialTemp(uint8_t value, int method);

// Global variables
extern TempCandidate tempCandidates[MAX_TEMP_CANDIDATES];
extern int numTempCandidates;
