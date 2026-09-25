#include <Arduino.h>

const int PIN_DAC = 25;      // salida al parlante
const int PIN_MIC = 34;      // entrada del microfono (A0)

const int N = 600;          // muestras a capturar
const int M = 200;           // muestras del chirp

// Chirp en frecuencia normalizada (ciclos por muestra)
const float F0 = 0.10;
const float F1 = 0.40;

uint8_t  chirp[M];
uint16_t datos[N];

void setup() {
  Serial.begin(115200);
  delay(500);

  // tabla del chirp, centrada en 128 porque el DAC es de 8 bits
  for (int n = 0; n < M; n++) {
    float fase = 2 * PI * (F0 * n + (F1 - F0) / (2.0 * M) * n * n);
    chirp[n] = 128 + 110 * sin(fase);
  }

  dacWrite(PIN_DAC, 0);        // parlante en reposo
  Serial.println("Escriba cualquier tecla y Enter para medir");
}

void loop() {
  if (!Serial.available()) return;
  while (Serial.available()) Serial.read();

  // emision y captura simultaneas, sin pausas
  unsigned long t0 = micros();
  for (int i = 0; i < N; i++) {
    dacWrite(PIN_DAC, i < M ? chirp[i] : 128);
    datos[i] = analogRead(PIN_MIC);
  }
  dacWrite(PIN_DAC, 0);        // apagar el DAC para que el parlante no zumbe
  unsigned long dt = micros() - t0;

  float fs = N * 1000000.0 / dt;

  // estadisticas de lo captado por el microfono
  int minimo = 4095, maximo = 0;
  for (int i = 0; i < N; i++) {
    if (datos[i] < minimo) minimo = datos[i];
    if (datos[i] > maximo) maximo = datos[i];
  }

  Serial.println("--- INICIO ---");
  Serial.print("fs_medida_Hz: ");
  Serial.println(fs, 1);
  Serial.print("chirp_f0_Hz: ");
  Serial.println(F0 * fs, 1);
  Serial.print("chirp_f1_Hz: ");
  Serial.println(F1 * fs, 1);
  Serial.print("duracion_chirp_ms: ");
  Serial.println(M * 1000.0 / fs, 2);
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