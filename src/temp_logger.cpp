#include "temp_logger.h"

// Initialize global variables
TempCandidate tempCandidates[MAX_TEMP_CANDIDATES];
int numTempCandidates = 0;

// Detailed CAN message logging specifically for temperature detection
void logDetailedCanMessage(twai_message_t message) {
  // Get current timestamp
  unsigned long timestamp = millis();
  
  // Log message header with timestamp
  Serial.print(timestamp);
  Serial.print(" | ID: 0x");
  Serial.print(message.identifier, HEX);
  Serial.print(" | Len: ");
  Serial.print(message.data_length_code);
  Serial.print(" | Data: ");
  
  // Log raw bytes in hex with extra space for readability
  for (int i = 0; i < message.data_length_code; i++) {
    Serial.printf("%02X ", message.data[i]);
  }
  
  // Add potential temperature decoded values
  Serial.print(" | Temps: ");
  for (int i = 0; i < message.data_length_code && i < 8; i++) {
    // Method 1: Standard OBD-II (value - 40)
    float tempMethod1 = message.data[i] - 40.0;
    // Method 2: Half-precision (value * 0.5 - 40)
    float tempMethod2 = (message.data[i] * 0.5) - 40.0;
    
    // Only show if in reasonable temperature range (-50°C to +100°C)
    if (tempMethod1 >= -50 && tempMethod1 <= 100) {
      Serial.printf("B%d: %.1f°C ", i, tempMethod1);
    }
    
    if (tempMethod2 >= -50 && tempMethod2 <= 100 && 
        (tempMethod2 != tempMethod1)) {  // Only show if different from method 1
      Serial.printf("B%d*: %.1f°C ", i, tempMethod2);
    }
  }
  
  Serial.println();
}

// Track CAN messages that might contain temperature data
void trackTemperatureCandidate(twai_message_t message) {
  // Skip messages that are too short
  if (message.data_length_code < 1) return;
  
  // Look for existing candidate
  int candidateIndex = -1;
  for (int i = 0; i < numTempCandidates; i++) {
    if (tempCandidates[i].id == message.identifier) {
      candidateIndex = i;
      break;
    }
  }
  
  // Create new candidate if not found and space available
  if (candidateIndex == -1) {
    if (numTempCandidates < MAX_TEMP_CANDIDATES) {
      candidateIndex = numTempCandidates++;
      tempCandidates[candidateIndex].id = message.identifier;
      tempCandidates[candidateIndex].length = message.data_length_code;
      tempCandidates[candidateIndex].lastChangeTime = millis();
      tempCandidates[candidateIndex].hasChanged = false;
      
      // Initialize change counts
      for (int i = 0; i < 8; i++) {
        tempCandidates[candidateIndex].changeCount[i] = 0;
        tempCandidates[candidateIndex].tempValues[i] = -999; // Invalid temp
      }
      
      // Copy initial data
      for (int i = 0; i < message.data_length_code && i < 8; i++) {
        tempCandidates[candidateIndex].lastData[i] = message.data[i];
        tempCandidates[candidateIndex].currentData[i] = message.data[i];
      }
    } else {
      // No space for new candidates
      return;
    }
  }
  
  // Update candidate and track changes
  bool changed = false;
  for (int i = 0; i < message.data_length_code && i < 8; i++) {
    // Check if value has changed
    if (tempCandidates[candidateIndex].currentData[i] != message.data[i]) {
      // Update change count for this byte
      tempCandidates[candidateIndex].changeCount[i]++;
      changed = true;
      
      // Copy last data before updating
      tempCandidates[candidateIndex].lastData[i] = tempCandidates[candidateIndex].currentData[i];
      
      // Update current data
      tempCandidates[candidateIndex].currentData[i] = message.data[i];
      
      // Try to decode as temperature
      tempCandidates[candidateIndex].tempValues[i] = decodePotentialTemp(message.data[i], 1);
    }
  }
  
  // Update change status
  if (changed) {
    tempCandidates[candidateIndex].hasChanged = true;
    tempCandidates[candidateIndex].lastChangeTime = millis();
    
    // Log the change with highlighting
    Serial.printf("★ CHANGE in 0x%03X: ", message.identifier);
    for (int i = 0; i < message.data_length_code && i < 8; i++) {
      if (tempCandidates[candidateIndex].lastData[i] != tempCandidates[candidateIndex].currentData[i]) {
        Serial.printf("[%02X→%02X] ", 
                     tempCandidates[candidateIndex].lastData[i],
                     tempCandidates[candidateIndex].currentData[i]);
                     
        // Add decoded temperature if in reasonable range
        float tempC = decodePotentialTemp(message.data[i], 1);
        if (tempC >= -50 && tempC <= 100) {
          Serial.printf("(%.1f°C) ", tempC);
        }
      } else {
        Serial.printf("%02X ", message.data[i]);
      }
    }
    Serial.println();
  }
}

// Display all temperature candidate messages and their change statistics
void displayTemperatureCandidates() {
  if (numTempCandidates == 0) {
    Serial.println("No temperature candidates tracked yet");
    return;
  }
  
  Serial.println("\n--- TEMPERATURE CANDIDATE MESSAGES ---");
  Serial.println("ID     | Changes | Most Active Bytes");
  Serial.println("-------|---------|------------------");
  
  for (int i = 0; i < numTempCandidates; i++) {
    // Calculate total changes and find most changed bytes
    int totalChanges = 0;
    int maxChanges = 0;
    int mostChangedBytes[3] = {-1, -1, -1};
    
    for (int j = 0; j < 8; j++) {
      totalChanges += tempCandidates[i].changeCount[j];
      
      // Track top 3 most changed bytes
      if (tempCandidates[i].changeCount[j] > maxChanges) {
        mostChangedBytes[2] = mostChangedBytes[1];
        mostChangedBytes[1] = mostChangedBytes[0];
        mostChangedBytes[0] = j;
        maxChanges = tempCandidates[i].changeCount[j];
      } else if (tempCandidates[i].changeCount[j] > 0) {
        if (mostChangedBytes[1] == -1 || 
            tempCandidates[i].changeCount[j] > tempCandidates[i].changeCount[mostChangedBytes[1]]) {
          mostChangedBytes[2] = mostChangedBytes[1];
          mostChangedBytes[1] = j;
        } else if (mostChangedBytes[2] == -1 || 
                  tempCandidates[i].changeCount[j] > tempCandidates[i].changeCount[mostChangedBytes[2]]) {
          mostChangedBytes[2] = j;
        }
      }
    }
    
    // Print candidate info
    Serial.printf("0x%03X | %7d | ", tempCandidates[i].id, totalChanges);
    
    // Print most changed bytes
    for (int j = 0; j < 3; j++) {
      if (mostChangedBytes[j] >= 0) {
        Serial.printf("B%d:%d ", mostChangedBytes[j], tempCandidates[i].changeCount[mostChangedBytes[j]]);
        // Print current temp value if available
        if (tempCandidates[i].tempValues[mostChangedBytes[j]] > -999) {
          Serial.printf("(%.1f°C) ", tempCandidates[i].tempValues[mostChangedBytes[j]]);
        }
      }
    }
    Serial.println();
  }
  Serial.println("-------------------------------------");
}

// Decode a byte as a potential temperature using different methods
float decodePotentialTemp(uint8_t value, int method) {
  switch (method) {
    case 1:
      // Standard OBD-II temperature encoding: value - 40°C
      return (value - 40.0);
    case 2:
      // Half-degree precision encoding: (value * 0.5) - 40°C
      return (value * 0.5) - 40.0;
    default:
      return -999; // Invalid method
  }
}
