#include <Arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
  tone(25, 2000);
  delay(500);
  noTone(25);
  delay(500);
}