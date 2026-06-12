/*
 * IR Sensor Controlled Solenoids
 * 
 * When FC-51 sensor detects an object:
 * - Fires both solenoids simultaneously
 * - Adjustable pulse duration
 * - Debounce to prevent multiple triggers
 * 
 * Hardware:
 * - FC-51 OUT → Pin A0 (or your chosen pin)
 * - Solenoid 1 → Pin 6
 * - Solenoid 2 → Pin 7
 */

#include <EEPROM.h>

// ===== PIN DEFINITIONS =====
const int SENSOR_PIN = A1;        // FC-51 output pin
const int SOLENOID1 = 6;          // Solenoid 1 control
const int SOLENOID2 = 7;          // Solenoid 2 control

// ===== SENSOR CONFIGURATION =====
const bool SENSOR_ACTIVE_LOW = true;  // true = LOW means object detected (most FC-51)
                                      // false = HIGH means object detected

// ===== SOLENOID CONFIGURATION =====
int pulseDuration = 300;           // How long solenoids fire (milliseconds)
const int MAX_PULSE_MS = 1000;     // Safety limit - never exceed 1 second
const int MIN_COOLDOWN_MS = 500;   // Minimum time between detections

// ===== DEBOUNCE SETTINGS =====
const unsigned long DEBOUNCE_DELAY = 50;    // Debounce time (milliseconds)
const unsigned long RETRIGGER_DELAY = 1000; // Wait before detecting again (milliseconds)

// ===== PROGRAM VARIABLES =====
bool lastSensorState = HIGH;       // Previous sensor reading
bool stableSensorState = HIGH;     // Debounced sensor reading
unsigned long lastDebounceTime = 0;
unsigned long lastFireTime = 0;
bool objectDetected = false;
int detectionCount = 0;

// ===== EEPROM ADDRESSES =====
const int EEPROM_PULSE_ADDR = 0;   // Store pulse duration in EEPROM

void setup() {
  Serial.begin(9600);
  
  // Configure pins
  pinMode(SENSOR_PIN, INPUT_PULLUP);  // Enable internal pull-up
  pinMode(SOLENOID1, OUTPUT);
  pinMode(SOLENOID2, OUTPUT);
  
  // Start with solenoids off
  digitalWrite(SOLENOID1, LOW);
  digitalWrite(SOLENOID2, LOW);
  
  // Load saved pulse duration from EEPROM
  int savedDuration = EEPROM.read(EEPROM_PULSE_ADDR);
  if (savedDuration >= 50 && savedDuration <= MAX_PULSE_MS) {
    pulseDuration = savedDuration;
  }
  
  printHeader();
  delay(1000);
}

void loop() {
  // Read sensor with debouncing
  int rawReading = digitalRead(SENSOR_PIN);
  
  // Debounce logic
  if (rawReading != lastSensorState) {
    lastDebounceTime = millis();
  }
  
  if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
    if (rawReading != stableSensorState) {
      stableSensorState = rawReading;
      
      // Check if object detected based on polarity
      if (isObjectDetected(stableSensorState)) {
        onObjectDetected();
      } else {
        onObjectCleared();
      }
    }
  }
  
  lastSensorState = rawReading;
  
  // Handle serial commands
  if (Serial.available()) {
    handleSerialCommand();
  }
  
  // Small delay for stability
  delay(10);
}

// Check if sensor indicates object detected
bool isObjectDetected(int reading) {
  if (SENSOR_ACTIVE_LOW) {
    return (reading == LOW);
  } else {
    return (reading == HIGH);
  }
}

// Called when object is detected
void onObjectDetected() {
  // Prevent rapid re-triggering
  if (millis() - lastFireTime < RETRIGGER_DELAY) {
    Serial.println("Object detected - ignoring (cooldown active)");
    return;
  }
  
  detectionCount++;
  lastFireTime = millis();
  
  Serial.println("");
  Serial.println("========================================");
  Serial.print("🔴 OBJECT DETECTED! (Detection #");
  Serial.print(detectionCount);
  Serial.println(")");
  Serial.print("   Firing solenoids for ");
  Serial.print(pulseDuration);
  Serial.println("ms");
  Serial.println("========================================");
  
  // Fire both solenoids simultaneously
  fireBothSolenoids(pulseDuration);
}

// Called when object is cleared (no longer detected)
void onObjectCleared() {
  Serial.println("🟢 PATH CLEAR - No object detected");
}

// Fire both solenoids at the exact same time
void fireBothSolenoids(int duration) {
  // Enforce maximum pulse duration (safety)
  if (duration > MAX_PULSE_MS) {
    duration = MAX_PULSE_MS;
    Serial.print("Duration limited to max: ");
    Serial.println(MAX_PULSE_MS);
  }
  
  // Turn BOTH on at the same time
  digitalWrite(SOLENOID1, HIGH);
  digitalWrite(SOLENOID2, HIGH);
  
  // Optional: Use direct port access for true simultaneity
  // PORTD |= (1 << 6) | (1 << 7);  // For pins 6 and 7 on PORTD
  
  // Wait for pulse duration
  delay(duration);
  
  // Turn BOTH off at the same time
  digitalWrite(SOLENOID1, LOW);
  digitalWrite(SOLENOID2, LOW);
  
  // PORTD &= ~((1 << 6) | (1 << 7));  // Direct port off
}

// Test fire solenoids (for testing without sensor)
void testFireSolenoids(int duration) {
  Serial.print("Test firing solenoids for ");
  Serial.print(duration);
  Serial.println("ms");
  fireBothSolenoids(duration);
}

// Print startup header
void printHeader() {
  Serial.println("========================================");
  Serial.println("   IR Sensor Controlled Solenoids");
  Serial.println("========================================");
  Serial.println("");
  Serial.print("Sensor pin: A0 (Physical pin 23)");
  Serial.print(" | Polarity: ");
  Serial.println(SENSOR_ACTIVE_LOW ? "Active LOW" : "Active HIGH");
  Serial.print("Solenoid 1: Pin ");
  Serial.println(SOLENOID1);
  Serial.print("Solenoid 2: Pin ");
  Serial.println(SOLENOID2);
  Serial.print("Current pulse duration: ");
  Serial.print(pulseDuration);
  Serial.println("ms");
  Serial.println("");
  Serial.println("--- Commands ---");
  Serial.println("  t     - Test fire solenoids");
  Serial.println("  d###  - Set pulse duration (e.g., d300 for 300ms)");
  Serial.println("  s     - Show current status");
  Serial.println("  r     - Reset detection counter");
  Serial.println("  h     - Show this help");
  Serial.println("");
  Serial.println("Place an object in front of the sensor to fire solenoids");
  Serial.println("========================================");
  Serial.println("");
}

// Handle serial commands
void handleSerialCommand() {
  char cmd = Serial.read();
  
  switch(cmd) {
    case 't':
    case 'T':
      Serial.println(">>> TEST MODE <<<");
      testFireSolenoids(pulseDuration);
      break;
      
    case 'd':
    case 'D':
      {
        int newDuration = Serial.parseInt();
        if (newDuration >= 50 && newDuration <= MAX_PULSE_MS) {
          pulseDuration = newDuration;
          EEPROM.update(EEPROM_PULSE_ADDR, pulseDuration);
          Serial.print("Pulse duration set to: ");
          Serial.print(pulseDuration);
          Serial.println("ms (saved to EEPROM)");
        } else {
          Serial.print("Invalid duration. Use 50-");
          Serial.print(MAX_PULSE_MS);
          Serial.println("ms");
        }
      }
      break;
      
    case 's':
    case 'S':
      Serial.println("--- Status ---");
      Serial.print("Sensor reading: ");
      int reading = digitalRead(SENSOR_PIN);
      Serial.println(reading == HIGH ? "HIGH" : "LOW");
      Serial.print("Object detected: ");
      Serial.println(isObjectDetected(reading) ? "YES" : "NO");
      Serial.print("Detection count: ");
      Serial.println(detectionCount);
      Serial.print("Pulse duration: ");
      Serial.print(pulseDuration);
      Serial.println("ms");
      Serial.print("Time since last fire: ");
      Serial.print(millis() - lastFireTime);
      Serial.println("ms");
      break;
      
    case 'r':
    case 'R':
      detectionCount = 0;
      Serial.println("Detection counter reset");
      break;
      
    case 'h':
    case 'H':
      printHeader();
      break;
      
    default:
      break;
  }
  
  // Clear any remaining serial buffer
  while (Serial.available()) {
    Serial.read();
  }
}