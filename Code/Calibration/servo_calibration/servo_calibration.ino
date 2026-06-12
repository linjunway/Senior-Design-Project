#include <Servo.h>

Servo myServo;
int currentAngle = 90;  // Start at servo's default position
const int stepSize = 1;

void setup() {
  Serial.begin(9600);
  myServo.attach(9);
  
  Serial.println("=== Servo Calibration Tool ===");
  Serial.println("This helps find the correct angles for your diverter");
  Serial.println("==================================================");
  Serial.println("Commands:");
  Serial.println("  + or = : Increase angle");
  Serial.println("  -      : Decrease angle");
  Serial.println("  s      : Save current position as STRAIGHT (0°)");
  Serial.println("  d      : Save current position as DIVERTED (45°)");
  Serial.println("  r      : Reset to servo's internal 90°");
  Serial.println();
  
  myServo.write(currentAngle);
  Serial.print("Current angle: ");
  Serial.println(currentAngle);
}

void loop() {
  if (Serial.available() > 0) {
    char command = Serial.read();
    
    if (command == '+') {
      currentAngle = min(currentAngle + stepSize, 180);
      myServo.write(currentAngle);
      Serial.print("Angle: ");
      Serial.println(currentAngle);
    }
    else if (command == '-') {
      currentAngle = max(currentAngle - stepSize, 0);
      myServo.write(currentAngle);
      Serial.print("Angle: ");
      Serial.println(currentAngle);
    }
    else if (command == 's') {
      Serial.print("STRAIGHT position saved at angle: ");
      Serial.println(currentAngle);
      Serial.println("Use this value for your 0° position");
    }
    else if (command == 'd') {
      Serial.print("DIVERTED position saved at angle: ");
      Serial.println(currentAngle);
      Serial.println("Use this value for your 45° position");
    }
    else if (command == 'r') {
      currentAngle = 90;
      myServo.write(currentAngle);
      Serial.println("Reset to internal 90°");
    }
  }
}