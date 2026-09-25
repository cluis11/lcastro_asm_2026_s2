#include <Arduino.h>

const int PIN_DAC = 25;
const int PIN_MIC = 34;

const int N = 2048;
const int M = 100;

const float FREQ_TONO = 2000.0;   // tono fijo, no barrido

const double FS_FIJA = 8000.0;
const uint64_t PERIODO_US = (uint64_t)(1000000.0 / FS_FIJA);

uint8_t  pulso[M];
uint16_t datos[N];

hw_timer_t *timer = NULL;
volatile bool tick = false;

void IRAM_ATTR onTimer() {
  tick = true;
}

void setup() {
  Serial.begin(115200);
  delay(500);

  for (int n = 0; n < M; n++) {
    float fase = 2 * PI * (FREQ_TONO / FS_FIJA) * n;
    pulso[n] = 128 + 110 * sin(fase);
  }

  dacWrite(PIN_DAC, 0);

  timer = timerBegin(0, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, PERIODO_US, true);
  timerAlarmEnable(timer);

  Serial.print("Pulso DAC fijo de ");
  Serial.print(FREQ_TONO, 0);
  Serial.print(" Hz, fs fija ");
  Serial.println(FS_FIJA, 1);
  Serial.println("Escriba cualquier tecla y Enter para medir");
}

void loop() {
  if (!Serial.available()) return;
  while (Serial.available()) Serial.read();

  while (!tick) {}
  tick = false;

  for (int i = 0; i < N; i++) {
    while (!tick) {}
    tick = false;

    dacWrite(PIN_DAC, i < M ? pulso[i] : 128);
    datos[i] = analogRead(PIN_MIC);
  }

  dacWrite(PIN_DAC, 0);

  int minimo = 4095, maximo = 0;
  for (int i = 0; i < N; i++) {
    if (datos[i] < minimo) minimo = datos[i];
    if (datos[i] > maximo) maximo = datos[i];
  }

  Serial.println("--- INICIO ---");
  Serial.print("fs_medida_Hz: ");
  Serial.println(FS_FIJA, 1);
  Serial.print("min: ");
  Serial.print(minimo);
  Serial.print("  max: ");
  Serial.print(maximo);
  Serial.print("  rango: ");
  Serial.println(maximo - minimo);
  for (int i = 0; i < N; i++) {
    Serial.println(datos[i]);
  }
  Serial.println("--- FIN ---");
}