#include <Arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
  tone(25, 500);   delay(1500); noTone(25); delay(300);
  tone(25, 1000);  delay(1500); noTone(25); delay(300);
  tone(25, 2000);  delay(1500); noTone(25); delay(300);
  tone(25, 3000);  delay(1500); noTone(25); delay(300);
  tone(25, 4000);  delay(1500); noTone(25); delay(300);
  tone(25, 6000);  delay(1500); noTone(25); delay(300);
  delay(3000);
}