#include <Arduino.h>

const int PIN_DAC = 25;
uint8_t tabla[64];

void setup() {
  for (int i = 0; i < 64; i++) {
    tabla[i] = 128 + 120 * sin(2 * PI * i / 64);
  }
}

void loop() {
  for (int i = 0; i < 64; i++) {
    dacWrite(PIN_DAC, tabla[i]);
  }
}