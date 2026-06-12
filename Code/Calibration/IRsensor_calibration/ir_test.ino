const int SENSOR_PIN = A1;

void setup() {
  pinMode(SENSOR_PIN, INPUT);
  Serial.begin(9600);
}

void loop() {
  if (digitalRead(SENSOR_PIN) == LOW) {
    Serial.println("Obstacle detected!");
  } else {
    Serial.println("Path clear");
  }
  delay(100);
}