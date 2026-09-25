#include <Arduino.h>

const int PIN_DAC = 25;
const int PIN_MIC = 34;

const int N = 2048;
const int M = 40;

const float F0 = 0.10;
const float F1 = 0.40;

const double FS_FIJA = 8000.0;   // frecuencia de muestreo FIJA, en Hz
const uint64_t PERIODO_US = (uint64_t)(1000000.0 / FS_FIJA);  // periodo entre muestras

uint8_t  chirp[M];
uint16_t datos[N];

hw_timer_t *timer = NULL;
volatile int indice = 0;
volatile bool capturando = false;
volatile bool listo = false;

void IRAM_ATTR onTimer() {
  if (!capturando) return;

  if (indice < M) {
    dacWrite(PIN_DAC, chirp[indice]);
  } else {
    dacWrite(PIN_DAC, 128);
  }

  datos[indice] = analogRead(PIN_MIC);
  indice++;

  if (indice >= N) {
    capturando = false;
    listo = true;
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  for (int n = 0; n < M; n++) {
    float fase = 2 * PI * (F0 * n + (F1 - F0) / (2.0 * M) * n * n);
    chirp[n] = 128 + 110 * sin(fase);
  }

  dacWrite(PIN_DAC, 0);

  // timer configurado para disparar exactamente cada PERIODO_US microsegundos
  timer = timerBegin(0, 80, true);        // prescaler 80: cada tick = 1 us (80MHz/80)
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, PERIODO_US, true);  // alarma periodica cada PERIODO_US

  Serial.print("fs fija configurada: ");
  Serial.println(FS_FIJA, 1);
  Serial.println("Escriba cualquier tecla y Enter para medir");
}

void loop() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();

    indice = 0;
    listo = false;
    capturando = true;
    timerAlarmEnable(timer);
  }

  if (listo) {
    timerAlarmDisable(timer);
    dacWrite(PIN_DAC, 0);
    listo = false;

    int minimo = 4095, maximo = 0;
    for (int i = 0; i < N; i++) {
      if (datos[i] < minimo) minimo = datos[i];
      if (datos[i] > maximo) maximo = datos[i];
    }

    Serial.println("--- INICIO ---");
    Serial.print("fs_medida_Hz: ");
    Serial.println(FS_FIJA, 1);   // ahora es la fs FIJA que configuramos, no medida
    Serial.print("chirp_f0_Hz: ");
    Serial.println(F0 * FS_FIJA, 1);
    Serial.print("chirp_f1_Hz: ");
    Serial.println(F1 * FS_FIJA, 1);
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
}