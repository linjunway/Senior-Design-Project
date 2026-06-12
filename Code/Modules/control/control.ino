#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Servo.h>

#define TCAADDR 0x70

// ===== COLOR SENSORS =====
Adafruit_TCS34725 sensor1 = Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_101MS,
  TCS34725_GAIN_4X
);

Adafruit_TCS34725 sensor2 = Adafruit_TCS34725(
  TCS34725_INTEGRATIONTIME_101MS,
  TCS34725_GAIN_4X
);

// ===== SERVOS =====
Servo servo1;
Servo servo2;

// ===== PINS =====
const int START_BUTTON = 4;
const int IR_SENSOR1 = A0;
const int IR_SENSOR2 = A1;
const int IR_SENSOR3 = A2;

const int SOLENOID1 = 6;
const int SOLENOID2 = 7;

const int SERVO1_PIN = 9;
const int SERVO2_PIN = 10;

const int MOTOR_AIN1 = 2;
const int MOTOR_AIN2 = 3;
const int MOTOR_PWM = 5;

// ===== SERVO POSITIONS =====
int servo1_orange = 176;
int servo1_white  = 134;

int servo2_orange = 88;
int servo2_white  = 137;

// ===== MOTOR =====
int motorSpeed = 120;

// ===== SOLENOIDS =====
int solenoidPulseDuration = 150;
const int MIN_COOLDOWN_MS = 1200;

unsigned long lastSolenoid1Fire = 0;
unsigned long lastSolenoid2Fire = 0;

// ===== TIMING =====
const int BALL_SETTLE_DELAY   = 200;
const int SENSOR_SETTLE_DELAY = 50;
const int GATE_MOVE_DELAY     = 180;
const int POST_FIRE_DELAY     = 20;

const unsigned long MOTOR_TIMEOUT = 10000;

// ===== THRESHOLDS =====
const uint16_t MIN_CLEAR_40 = 1000;
const float MIN_RG_40 = 1.7;
const float MAX_RG_40 = 1.35;

const uint16_t MAX_CLEAR_38 = 9200;
const float MIN_RG_38 = 1.5;
const float MAX_RG_38 = 1.35;

// ===== STATE =====
bool motorRunning = false;
bool processingBall40 = false;
bool processingBall38 = false;

unsigned long lastActivityTime = 0;

int lastServo1Position = -1;
int lastServo2Position = -1;

// =====================================================
// SETUP
// =====================================================
void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(START_BUTTON, INPUT_PULLUP);

  pinMode(IR_SENSOR1, INPUT_PULLUP);
  pinMode(IR_SENSOR2, INPUT_PULLUP);
  pinMode(IR_SENSOR3, INPUT_PULLUP);

  pinMode(SOLENOID1, OUTPUT);
  pinMode(SOLENOID2, OUTPUT);

  pinMode(MOTOR_AIN1, OUTPUT);
  pinMode(MOTOR_AIN2, OUTPUT);
  pinMode(MOTOR_PWM, OUTPUT);

  digitalWrite(SOLENOID1, LOW);
  digitalWrite(SOLENOID2, LOW);

  servo1.attach(SERVO1_PIN);
  servo2.attach(SERVO2_PIN);

  servo1.write(servo1_orange);
  servo2.write(servo2_orange);

  lastServo1Position = servo1_orange;
  lastServo2Position = servo2_orange;

  motorStop();

  tcaselect(0);
  sensor1.begin();

  tcaselect(1);
  sensor2.begin();

  Serial.println("System ready");
}

// =====================================================
// MAIN LOOP
// =====================================================
void loop() {
  if (digitalRead(START_BUTTON) == LOW && !motorRunning) {
    startMotor();
    delay(500);
  }

  if (motorRunning) {
    checkMotorStopCondition();
  }

  runColorSorting();

  delay(20);
}

// =====================================================
// SORTING
// =====================================================
void runColorSorting() {
  bool ball40 = (digitalRead(IR_SENSOR1) == LOW);
  bool ball38 = (digitalRead(IR_SENSOR2) == LOW);

  // Any detected ball = activity
  if (ball40 || ball38) {
    lastActivityTime = millis();
  }

  if (ball40 && !processingBall40) {
    processingBall40 = true;
    process40mmBall();
    processingBall40 = false;
  }

  if (ball38 && !processingBall38) {
    processingBall38 = true;
    process38mmBall();
    processingBall38 = false;
  }
}

void process40mmBall() {
  Serial.println("40mm ball detected");
  lastActivityTime = millis();

  delay(BALL_SETTLE_DELAY);

  tcaselect(0);

  uint16_t r, g, b, c;
  readStableColor(sensor1, &r, &g, &b, &c);

  float rg = (float)r / g;

  printSensorData("40mm", r, g, b, c, rg);

  if (c <= MIN_CLEAR_40) return;

  bool isOrange = (rg > MIN_RG_40);
  bool isWhite  = (rg < MAX_RG_40);

  if (!(isOrange || isWhite)) {
    Serial.println("40mm unknown");
    return;
  }

  int targetPos = isOrange ? servo1_orange : servo1_white;

  Serial.print("40mm classified: ");
  Serial.println(isOrange ? "ORANGE" : "WHITE");

  if (targetPos != lastServo1Position) {
    servo1.write(targetPos);
    lastServo1Position = targetPos;
  }

  delay(GATE_MOVE_DELAY);

  fireSolenoid(SOLENOID1, 1);
  delay(POST_FIRE_DELAY);
}

void process38mmBall() {
  Serial.println("38mm ball detected");
  lastActivityTime = millis();

  delay(BALL_SETTLE_DELAY);

  tcaselect(1);

  uint16_t r, g, b, c;
  readStableColor(sensor2, &r, &g, &b, &c);

  float rg = (float)r / g;

  printSensorData("38mm", r, g, b, c, rg);

  if (c >= MAX_CLEAR_38) return;

  bool isOrange = (rg > MIN_RG_38);
  bool isWhite  = (rg < MAX_RG_38);

  if (!(isOrange || isWhite)) {
    Serial.println("38mm unknown");
    return;
  }

  int targetPos = isOrange ? servo2_orange : servo2_white;

  Serial.print("38mm classified: ");
  Serial.println(isOrange ? "ORANGE" : "WHITE");

  if (targetPos != lastServo2Position) {
    servo2.write(targetPos);
    lastServo2Position = targetPos;
  }

  delay(GATE_MOVE_DELAY);   // ALWAYS wait

  fireSolenoid(SOLENOID2, 2);
  delay(POST_FIRE_DELAY);
}

// =====================================================
// SENSOR READING
// =====================================================
void readStableColor(
  Adafruit_TCS34725& sensor,
  uint16_t* r,
  uint16_t* g,
  uint16_t* b,
  uint16_t* c
) {
  uint16_t rt, gt, bt, ct;
  uint32_t rSum = 0, gSum = 0, bSum = 0, cSum = 0;

  // Throw away stale frame
  sensor.getRawData(&rt, &gt, &bt, &ct);
  delay(SENSOR_SETTLE_DELAY);

  for (int i = 0; i < 3; i++) {
    sensor.getRawData(&rt, &gt, &bt, &ct);

    rSum += rt;
    gSum += gt;
    bSum += bt;
    cSum += ct;

    delay(20);
  }

  *r = rSum / 3;
  *g = gSum / 3;
  *b = bSum / 3;
  *c = cSum / 3;
}

void printSensorData(
  const char* label,
  uint16_t r,
  uint16_t g,
  uint16_t b,
  uint16_t c,
  float rg
) {
  Serial.println("----- SENSOR DATA -----");
  Serial.println(label);
  Serial.print("R: "); Serial.println(r);
  Serial.print("G: "); Serial.println(g);
  Serial.print("B: "); Serial.println(b);
  Serial.print("C: "); Serial.println(c);
  Serial.print("R/G: "); Serial.println(rg, 3);
  Serial.println("-----------------------");
}

// =====================================================
// SOLENOID
// =====================================================
bool canFireSolenoid(int num) {
  unsigned long now = millis();
  unsigned long lastFire = (num == 1) ? lastSolenoid1Fire : lastSolenoid2Fire;
  return (now - lastFire >= MIN_COOLDOWN_MS);
}

void fireSolenoid(int pin, int num) {
  if (!canFireSolenoid(num)) {
    Serial.println("Solenoid cooling");
    return;
  }

  digitalWrite(pin, HIGH);
  delay(solenoidPulseDuration);
  digitalWrite(pin, LOW);

  if (num == 1) lastSolenoid1Fire = millis();
  else lastSolenoid2Fire = millis();

  lastActivityTime = millis();
}

// =====================================================
// MOTOR CONTROL
// =====================================================
void startMotor() {
  motorRunning = true;
  lastActivityTime = millis();

  digitalWrite(MOTOR_AIN1, HIGH);
  digitalWrite(MOTOR_AIN2, LOW);
  analogWrite(MOTOR_PWM, motorSpeed);

  Serial.println("Motor started");
}

void motorStop() {
  digitalWrite(MOTOR_AIN1, LOW);
  digitalWrite(MOTOR_AIN2, LOW);
  analogWrite(MOTOR_PWM, 0);
}

void checkMotorStopCondition() {
  unsigned long now = millis();

  bool ir3Blocked = (digitalRead(IR_SENSOR3) == LOW);

  // Exit sensor activity
  if (ir3Blocked) {
    lastActivityTime = now;
  }

  if (motorRunning && (now - lastActivityTime >= MOTOR_TIMEOUT)) {
    motorStop();
    motorRunning = false;
    Serial.println("Motor stopped - no activity for 10 seconds");
  }
}

// =====================================================
// I2C MUX
// =====================================================
void tcaselect(uint8_t channel) {
  if (channel > 7) return;

  Wire.beginTransmission(TCAADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();

  delay(SENSOR_SETTLE_DELAY);
}