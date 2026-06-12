#include <Wire.h>
#include "Adafruit_TCS34725.h"

#define TCAADDR 0x70

// Create sensor instances
Adafruit_TCS34725 sensor1 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 sensor2 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);

// Thresholds for 40mm balls (Sensor 1)
struct Thresholds40mm {
  uint16_t minClear = 1000;      // C > 1000 = 40mm ball present... NOTE: this value only works in well lit environment. Will need to reduce in darker lighting
  float minRG = 1.8;             // R/G > 1.8 = ORANGE
  float maxRG = 1.35;            // R/G < 1.35 = WHITE
  uint16_t minRed = 3000;        // Red > 3000 confirms 40mm
};

// Thresholds for 38mm balls (Sensor 2)
struct Thresholds38mm {
  uint16_t maxClear = 9200;      // C < 9200 = 38mm ball present... NOTE: prev val of 4k had issues if the ball shifted closer to the sensor
  float minRG = 1.5;             // R/G > 1.5 = ORANGE
  float maxRG = 1.35;            // R/G < 1.35 = WHITE
  uint16_t minRed = 800;         // Red > 800 confirms detection
};

Thresholds40mm t40;
Thresholds38mm t38;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Initialize sensors
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
  
  Serial.println("\n=== Ball Detection System ===\n");
}

void loop() {
  // Read Sensor 1 (40mm detection)
  tcaselect(0);
  uint16_t r1, g1, b1, c1;
  sensor1.getRawData(&r1, &g1, &b1, &c1);
  float rg1 = (float)r1 / g1;
  
  // Read Sensor 2 (38mm detection)
  tcaselect(1);
  uint16_t r2, g2, b2, c2;
  sensor2.getRawData(&r2, &g2, &b2, &c2);
  float rg2 = (float)r2 / g2;
  
  // Classify each sensor
  String result1 = classify40mm(r1, g1, c1, rg1);
  String result2 = classify38mm(r2, g2, c2, rg2);
  
  // Print results
  printResults(r1, g1, b1, c1, rg1, "40mm Sensor", result1);
  printResults(r2, g2, b2, c2, rg2, "38mm Sensor", result2);
  
  Serial.println("----------------------------------------\n");
  
  delay(500);
}

// Classify 40mm balls (Sensor 1)
String classify40mm(uint16_t r, uint16_t g, uint16_t c, float rg) {
  // Empty tube check
  if (c < 400) {
    return "EMPTY";
  }
  
  // Check if 40mm ball is present (high Clear value)
  if (c > t40.minClear) {
    if (rg > t40.minRG) {
      return "40mm ORANGE BALL";
    } else if (rg < t40.maxRG) {
      return "40mm WHITE BALL";
    }
  }
  
  return "Unknown";
}

// Classify 38mm balls (Sensor 2)
String classify38mm(uint16_t r, uint16_t g, uint16_t c, float rg) {
  // Empty tube check
  if (c < 400) {
    return "EMPTY";
  }
  
  // Check if 38mm ball is present (moderate Clear value)
  if (c < t38.maxClear) {
    if (rg > t38.minRG) {
      return "38mm ORANGE BALL";
    } else if (rg < t38.maxRG) {
      return "38mm WHITE BALL";
    }
  }
  
  return "Unknown";
}

void printResults(uint16_t r, uint16_t g, uint16_t b, uint16_t c, float rg, String sensorName, String result) {
  Serial.print(sensorName);
  Serial.print(": ");
  Serial.print(result);
  Serial.print(" (R:");
  Serial.print(r);
  Serial.print(" G:");
  Serial.print(g);
  Serial.print(" B:");
  Serial.print(b);
  Serial.print(" C:");
  Serial.print(c);
  Serial.print(" R/G:");
  Serial.print(rg, 2);
  Serial.println(")");
}

void tcaselect(uint8_t channel) {
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
  delay(2);
}