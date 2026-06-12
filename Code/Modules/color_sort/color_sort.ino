#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Servo.h>

#define TCAADDR 0x70

// ===== CREATE SENSOR INSTANCES =====
Adafruit_TCS34725 sensor1 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 sensor2 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);

// ===== CREATE SERVO OBJECTS =====
Servo servo1;  // Servo for 40mm ball mechanism
Servo servo2;  // Servo for 38mm ball mechanism

// ===== PIN DEFINITIONS =====
const int IR_SENSOR1 = A0;   // IR sensor for 40mm ball detection zone
const int IR_SENSOR2 = A1;   // IR sensor for 38mm ball detection zone
const int SOLENOID1 = 6;     // Solenoid for 40mm ball mechanism
const int SOLENOID2 = 7;     // Solenoid for 38mm ball mechanism
const int SERVO1_PIN = 9;    // Servo for 40mm ball mechanism
const int SERVO2_PIN = 10;   // Servo for 38mm ball mechanism

// ===== CUSTOM SERVO POSITIONS =====
int servo1_orange = 178;      // Position for ORANGE balls (0-180)
int servo1_white = 133;       // Position for WHITE balls (0-180)
int servo2_orange = 88;       // Position for ORANGE balls (0-180)
int servo2_white = 137;       // Position for WHITE balls (0-180)

// ===== SOLENOID THERMAL PROTECTION SETTINGS =====
// These prevent overheating and damage
int solenoidPulseDuration = 150;      // REDUCED: 150ms (was 300ms) - shorter is cooler
const int MIN_COOLDOWN_MS = 2000;     // Minimum 2 seconds between ANY solenoid fires
const int MAX_PULSES_PER_MINUTE = 15; // Maximum 15 pulses per minute (1 every 4 seconds)
const int MAX_PULSE_DURATION_MS = 300; // NEVER exceed 300ms pulse (safety)

// ===== SOLENOID TRACKING VARIABLES =====
unsigned long lastSolenoid1Fire = 0;
unsigned long lastSolenoid2Fire = 0;
unsigned long solenoid1FireTimes[60] = {0};  // Store last 60 fire times (1 minute)
unsigned long solenoid2FireTimes[60] = {0};
int solenoid1PulseCount = 0;
int solenoid2PulseCount = 0;

// ===== COLOR SENSOR THRESHOLDS =====
struct Thresholds40mm {
  uint16_t minClear = 1000;
  float minRG = 1.8;
  float maxRG = 1.35;
};

struct Thresholds38mm {
  uint16_t maxClear = 9200;
  float minRG = 1.5;
  float maxRG = 1.35;
};

Thresholds40mm t40;
Thresholds38mm t38;

// ===== TRACK LAST POSITIONS =====
int lastServo1Position = -1;
int lastServo2Position = -1;
String lastColor40 = "";
String lastColor38 = "";

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  pinMode(IR_SENSOR1, INPUT_PULLUP);
  pinMode(IR_SENSOR2, INPUT_PULLUP);
  
  pinMode(SOLENOID1, OUTPUT);
  pinMode(SOLENOID2, OUTPUT);
  digitalWrite(SOLENOID1, LOW);
  digitalWrite(SOLENOID2, LOW);
  
  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);
  servo1.write(servo1_orange);
  servo2.write(servo2_orange);
  lastServo1Position = servo1_orange;
  lastServo2Position = servo2_orange;
  
  tcaselect(0);
  if (sensor1.begin()) {
    sensor1.setIntegrationTime(TCS34725_INTEGRATIONTIME_101MS);
    sensor1.setGain(TCS34725_GAIN_4X);
    Serial.println("Sensor 1 (40mm detector) ready");
  }
  
  tcaselect(1);
  if (sensor2.begin()) {
    sensor2.setIntegrationTime(TCS34725_INTEGRATIONTIME_101MS);
    sensor2.setGain(TCS34725_GAIN_4X);
    Serial.println("Sensor 2 (38mm detector) ready");
  }
  
  Serial.println("\n=== Ball Sorting System with Thermal Protection ===\n");
  Serial.print("Solenoid pulse duration: ");
  Serial.print(solenoidPulseDuration);
  Serial.println("ms");
  Serial.print("Minimum cooldown: ");
  Serial.print(MIN_COOLDOWN_MS / 1000);
  Serial.println(" seconds");
  Serial.print("Max pulses per minute: ");
  Serial.println(MAX_PULSES_PER_MINUTE);
  Serial.println();
}

void loop() {
  // Read IR sensors
  bool ballPresent40 = (digitalRead(IR_SENSOR1) == LOW);
  bool ballPresent38 = (digitalRead(IR_SENSOR2) == LOW);
  
  // Read color sensors
  tcaselect(0);
  uint16_t r1, g1, b1, c1;
  sensor1.getRawData(&r1, &g1, &b1, &c1);
  float rg1 = (float)r1 / g1;
  
  tcaselect(1);
  uint16_t r2, g2, b2, c2;
  sensor2.getRawData(&r2, &g2, &b2, &c2);
  float rg2 = (float)r2 / g2;
  
  // Classify balls
  String color40 = "EMPTY";
  String color38 = "EMPTY";
  
  if (ballPresent40) {
    color40 = classify40mm(r1, g1, c1, rg1);
  }
  
  if (ballPresent38) {
    color38 = classify38mm(r2, g2, c2, rg2);
  }
  
  // Handle detections with thermal protection
  if (ballPresent40 && (color40 == "40mm ORANGE" || color40 == "40mm WHITE")) {
    handle40mmBall(color40);
  }
  
  if (ballPresent38 && (color38 == "38mm ORANGE" || color38 == "38mm WHITE")) {
    handle38mmBall(color38);
  }
  
  printDebug(ballPresent40, ballPresent38, 
             r1, g1, b1, c1, rg1, color40,
             r2, g2, b2, c2, rg2, color38);
  
  delay(100);
}

// ===== THERMAL PROTECTION FUNCTIONS =====

// Check if solenoid can fire based on cooldown and rate limits
bool canFireSolenoid(int solenoidNum) {
  unsigned long now = millis();
  unsigned long lastFire = (solenoidNum == 1) ? lastSolenoid1Fire : lastSolenoid2Fire;
  
  // Check cooldown period
  if (now - lastFire < MIN_COOLDOWN_MS) {
    unsigned long remaining = MIN_COOLDOWN_MS - (now - lastFire);
    Serial.print("  ⚠️ Solenoid ");
    Serial.print(solenoidNum);
    Serial.print(" cooling... ");
    Serial.print(remaining / 1000);
    Serial.println(" seconds remaining");
    return false;
  }
  
  // Check pulses per minute limit
  int pulseCount = (solenoidNum == 1) ? solenoid1PulseCount : solenoid2PulseCount;
  if (pulseCount >= MAX_PULSES_PER_MINUTE) {
    Serial.print("  ⚠️ Solenoid ");
    Serial.print(solenoidNum);
    Serial.println(" reached max pulses per minute! Waiting...");
    return false;
  }
  
  return true;
}

// Record a solenoid fire for rate tracking
void recordSolenoidFire(int solenoidNum) {
  unsigned long now = millis();
  
  if (solenoidNum == 1) {
    lastSolenoid1Fire = now;
    // Shift pulse history and add new pulse
    for (int i = 59; i > 0; i--) {
      solenoid1FireTimes[i] = solenoid1FireTimes[i-1];
    }
    solenoid1FireTimes[0] = now;
    solenoid1PulseCount = countPulsesInLastMinute(solenoid1FireTimes);
  } else {
    lastSolenoid2Fire = now;
    for (int i = 59; i > 0; i--) {
      solenoid2FireTimes[i] = solenoid2FireTimes[i-1];
    }
    solenoid2FireTimes[0] = now;
    solenoid2PulseCount = countPulsesInLastMinute(solenoid2FireTimes);
  }
}

// Count how many pulses occurred in the last 60 seconds
int countPulsesInLastMinute(unsigned long fireTimes[]) {
  unsigned long now = millis();
  int count = 0;
  for (int i = 0; i < 60; i++) {
    if (fireTimes[i] > 0 && (now - fireTimes[i]) <= 60000) {
      count++;
    }
  }
  return count;
}

// Safe solenoid fire with thermal protection
bool safeFireSolenoid(int pin, int solenoidNum, int requestedDuration) {
  // Enforce max pulse duration
  int duration = requestedDuration;
  if (duration > MAX_PULSE_DURATION_MS) {
    duration = MAX_PULSE_DURATION_MS;
    Serial.print("  ⚠️ Pulse duration reduced to ");
    Serial.print(duration);
    Serial.println("ms (safety limit)");
  }
  
  // Check if firing is allowed
  if (!canFireSolenoid(solenoidNum)) {
    return false;
  }
  
  // Fire the solenoid
  digitalWrite(pin, HIGH);
  delay(duration);
  digitalWrite(pin, LOW);
  
  // Record this fire for thermal tracking
  recordSolenoidFire(solenoidNum);
  
  Serial.print("  → Solenoid ");
  Serial.print(solenoidNum);
  Serial.print(" fired for ");
  Serial.print(duration);
  Serial.print("ms | Pulses in last minute: ");
  Serial.print((solenoidNum == 1) ? solenoid1PulseCount : solenoid2PulseCount);
  Serial.print("/");
  Serial.println(MAX_PULSES_PER_MINUTE);
  
  return true;
}

// ===== CLASSIFICATION FUNCTIONS =====

String classify40mm(uint16_t r, uint16_t g, uint16_t c, float rg) {
  if (c < t40.minClear) return "EMPTY";
  if (rg > t40.minRG) return "40mm ORANGE";
  if (rg < t40.maxRG) return "40mm WHITE";
  return "UNKNOWN";
}

String classify38mm(uint16_t r, uint16_t g, uint16_t c, float rg) {
  if (c > t38.maxClear) return "EMPTY";
  if (rg > t38.minRG) return "38mm ORANGE";
  if (rg < t38.maxRG) return "38mm WHITE";
  return "UNKNOWN";
}

// ===== ACTION FUNCTIONS =====

void handle40mmBall(String color) {
  Serial.print("40mm ball detected: ");
  Serial.println(color);
  
  // Determine target position based on color
  int targetPosition = (color == "40mm ORANGE") ? servo1_orange : servo1_white;
  
  // Move servo only if position changed
  if (targetPosition != lastServo1Position) {
    Serial.print("  → Moving servo to ");
    Serial.print(targetPosition);
    Serial.println("°");
    servo1.write(targetPosition);
    lastServo1Position = targetPosition;
    delay(200);
  } else {
    Serial.println("  → Servo already in correct position");
  }
  
  // Fire solenoid with thermal protection
  safeFireSolenoid(SOLENOID1, 1, solenoidPulseDuration);
  
  lastColor40 = color;
}

void handle38mmBall(String color) {
  Serial.print("38mm ball detected: ");
  Serial.println(color);
  
  int targetPosition = (color == "38mm ORANGE") ? servo2_orange : servo2_white;
  
  if (targetPosition != lastServo2Position) {
    Serial.print("  → Moving servo to ");
    Serial.print(targetPosition);
    Serial.println("°");
    servo2.write(targetPosition);
    lastServo2Position = targetPosition;
    delay(200);
  } else {
    Serial.println("  → Servo already in correct position");
  }
  
  // Fire solenoid with thermal protection
  safeFireSolenoid(SOLENOID2, 2, solenoidPulseDuration);
  
  lastColor38 = color;
}

// ===== HELPER FUNCTIONS =====

void printDebug(bool ball40, bool ball38,
                uint16_t r1, uint16_t g1, uint16_t b1, uint16_t c1, float rg1, String color40,
                uint16_t r2, uint16_t g2, uint16_t b2, uint16_t c2, float rg2, String color38) {
  
  static unsigned long lastPrint = 0;
  
  if (millis() - lastPrint > 500) {
    Serial.println("========================================");
    Serial.print("IR: 40mm:");
    Serial.print(ball40 ? "BALL" : "empty");
    Serial.print(" | 38mm:");
    Serial.println(ball38 ? "BALL" : "empty");
    
    Serial.print("40mm: ");
    Serial.print(color40);
    if (ball40) {
      Serial.print(" (R/G:");
      Serial.print(rg1, 2);
      Serial.print(" C:");
      Serial.print(c1);
      Serial.print(")");
    }
    Serial.println();
    
    Serial.print("38mm: ");
    Serial.print(color38);
    if (ball38) {
      Serial.print(" (R/G:");
      Serial.print(rg2, 2);
      Serial.print(" C:");
      Serial.print(c2);
      Serial.print(")");
    }
    Serial.println();
    
    Serial.print("Servos: 40mm:");
    Serial.print(lastServo1Position);
    Serial.print("° | 38mm:");
    Serial.println(lastServo2Position);
    
    // Show thermal status
    Serial.print("Thermal: S1:");
    Serial.print((millis() - lastSolenoid1Fire) < MIN_COOLDOWN_MS ? "COOLING" : "ready");
    Serial.print(" (");
    Serial.print(solenoid1PulseCount);
    Serial.print("/min) | S2:");
    Serial.print((millis() - lastSolenoid2Fire) < MIN_COOLDOWN_MS ? "COOLING" : "ready");
    Serial.print(" (");
    Serial.print(solenoid2PulseCount);
    Serial.println("/min)");
    
    Serial.println("========================================\n");
    lastPrint = millis();
  }
}

void tcaselect(uint8_t channel) {
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
  delay(2);
}