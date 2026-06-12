#include <Wire.h>
#include "Adafruit_TCS34725.h"

#define TCAADDR 0x70

// Create sensor instances
Adafruit_TCS34725 sensor1 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);
Adafruit_TCS34725 sensor2 = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_101MS, TCS34725_GAIN_4X);

// Store readings
struct SensorData {
  uint16_t r, g, b, c;
  bool valid;
};

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  Serial.println("Initializing both sensors...");
  
  // Initialize Sensor 1 on Channel 0
  if (initSensor(0, sensor1, "Sensor 1")) {
    Serial.println("✓ Sensor 1 (Channel 0) ready");
  } else {
    Serial.println("✗ Sensor 1 (Channel 0) failed");
  }
  
  // Initialize Sensor 2 on Channel 1
  if (initSensor(1, sensor2, "Sensor 2")) {
    Serial.println("✓ Sensor 2 (Channel 1) ready");
  } else {
    Serial.println("✗ Sensor 2 (Channel 1) failed");
  }
  
  Serial.println("\n--- Reading both sensors ---\n");
}

void loop() {
  // Read both sensors
  SensorData data1 = readSensor(0, sensor1);
  SensorData data2 = readSensor(1, sensor2);
  
  // Print results side by side
  Serial.println("========================================");
  Serial.print("Sensor 1 (Ch0)"); 
  Serial.print("          Sensor 2 (Ch1)");
  Serial.println();
  Serial.print("R: "); Serial.print(data1.r);
  Serial.print("                 R: "); Serial.println(data2.r);
  
  Serial.print("G: "); Serial.print(data1.g);
  Serial.print("                 G: "); Serial.println(data2.g);
  
  Serial.print("B: "); Serial.print(data1.b);
  Serial.print("                 B: "); Serial.println(data2.b);
  
  Serial.print("C: "); Serial.print(data1.c);
  Serial.print("                 C: "); Serial.println(data2.c);
  
  // Optional: Add color detection
  printColorDetection(data1, "S1");
  printColorDetection(data2, "S2");
  
  delay(1000);  // Adjust as needed
}

// Initialize sensor on specific channel
bool initSensor(uint8_t channel, Adafruit_TCS34725 &sensor, const char* name) {
  tcaselect(channel);
  delay(10);
  
  if (sensor.begin()) {
    return true;
  }
  return false;
}

// Read sensor data from specific channel
SensorData readSensor(uint8_t channel, Adafruit_TCS34725 &sensor) {
  SensorData data;
  data.valid = false;
  
  // Select the channel
  tcaselect(channel);
  delay(5);  // Allow channel to settle
  
  // Check if sensor responds
  Wire.beginTransmission(0x29);
  if (Wire.endTransmission() != 0) {
    data.valid = false;
    return data;
  }
  
  // Read raw data
  sensor.getRawData(&data.r, &data.g, &data.b, &data.c);
  data.valid = true;
  
  return data;
}

// Select channel on TCA9548A
void tcaselect(uint8_t channel) {
  if (channel > 7) return;
  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
  delay(2);
}

// Optional: Print color detection
void printColorDetection(SensorData data, const char* label) {
  if (!data.valid) {
    Serial.print(label);
    Serial.println(": NO SENSOR");
    return;
  }
  
  float rNorm = (float)data.r / data.c;
  float gNorm = (float)data.g / data.c;
  float bNorm = (float)data.b / data.c;
  
  Serial.print(label);
  Serial.print(" Color: ");
  
  if (data.c < 500) {
    Serial.println("DARK");
  } else if (rNorm > gNorm && rNorm > bNorm) {
    Serial.println("RED");
  } else if (gNorm > rNorm && gNorm > bNorm) {
    Serial.println("GREEN");
  } else if (bNorm > rNorm && bNorm > gNorm) {
    Serial.println("BLUE");
  } else if (rNorm > 0.35 && gNorm > 0.30 && bNorm > 0.20) {
    Serial.println("WHITE");
  } else {
    Serial.println("MIXED");
  }
}