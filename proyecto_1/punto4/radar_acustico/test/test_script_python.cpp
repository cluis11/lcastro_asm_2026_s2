#include <Arduino.h>

// =====================================================
// CONFIGURACION
// =====================================================

const int PIN_DAC = 25;   // DAC1 -> entrada L del PAM8403
const int PIN_MIC = 34;   // MAX4466 OUT

const uint32_t FS = 8000;
const uint32_t PERIODO_US = 1000000UL / FS;

const int N = 2048;

// Chirp
const int M = 15;
const float F0 = 2000.0f;
const float F1 = 3500.0f;
const float A = 5.0f;

// Centro del DAC
const uint8_t DAC_CENTRO = 128;

uint8_t chirp[M];
uint16_t datos[N];

// =====================================================
// TIMER
// =====================================================

hw_timer_t *timer = NULL;
volatile uint32_t ticks = 0;

void IRAM_ATTR onTimer()
{
    ticks++;
}

inline void esperarTick()
{
    while (ticks == 0) {
    }

    ticks--;
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);
    delay(500);

    // -------------------------------------------------
    // GENERAR CHIRP
    // -------------------------------------------------

    float duracionBarrido =
        (float)(M - 1) / (float)FS;

    float k =
        (F1 - F0) / duracionBarrido;

    for (int n = 0; n < M; n++)
    {
        float t =
            (float)n / (float)FS;

        float fase =
            2.0f * PI *
            (
                F0 * t +
                0.5f * k * t * t
            );

        // Ventana Hann
        float ventana =
            0.5f *
            (
                1.0f -
                cosf(
                    2.0f * PI *
                    n /
                    (M - 1)
                )
            );

        float valor =
            (float)DAC_CENTRO +
            A *
            ventana *
            sinf(fase);

        chirp[n] =
            (uint8_t)constrain(
                (int)roundf(valor),
                0,
                255
            );
    }

    // -------------------------------------------------
    // DAC
    // -------------------------------------------------

    // Reposo en mitad de escala
    dacWrite(PIN_DAC, DAC_CENTRO);

    // -------------------------------------------------
    // ADC
    // -------------------------------------------------

    analogReadResolution(12);

    analogSetPinAttenuation(
        PIN_MIC,
        ADC_11db
    );

    // -------------------------------------------------
    // TIMER
    // -------------------------------------------------

    timer = timerBegin(
        0,
        80,
        true
    );

    timerAttachInterrupt(
        timer,
        &onTimer,
        true
    );

    timerAlarmWrite(
        timer,
        PERIODO_US,
        true
    );

    timerAlarmEnable(timer);

    Serial.println();
    Serial.println("Sistema listo.");
    Serial.println("DAC GPIO25 -> PAM8403");
    Serial.println("Enviar x para medir.");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    if (!Serial.available())
        return;

    // Vaciar buffer serial
    while (Serial.available())
        Serial.read();

    // Reposo DAC
    dacWrite(
        PIN_DAC,
        DAC_CENTRO
    );

    ticks = 0;

    esperarTick();

    uint32_t tInicio = micros();

    // =================================================
    // CAPTURA
    // =================================================

    for (int i = 0; i < N; i++)
    {
        esperarTick();

        // ---------------------------------------------
        // TRANSMISION
        // ---------------------------------------------

        if (i < M)
        {
            // Chirp
            dacWrite(
                PIN_DAC,
                chirp[i]
            );
        }
        else if (i==M)
        {
            // Reposo.
            // IMPORTANTE: no ponemos 0.
            dacWrite(
                PIN_DAC,
                DAC_CENTRO
            );
        }

        // ---------------------------------------------
        // RECEPCION
        // ---------------------------------------------

        datos[i] =
            analogRead(PIN_MIC);
    }

    uint32_t tFin = micros();

    // Mantener DAC centrado al terminar
    dacWrite(
        PIN_DAC,
        DAC_CENTRO
    );

    // =================================================
    // Fs REAL
    // =================================================

    float tiempo_s =
        (float)(tFin - tInicio) /
        1000000.0f;

    float fsReal =
        (float)N /
        tiempo_s;

    // =================================================
    // ESTADISTICAS
    // =================================================

    uint16_t minimo = 4095;
    uint16_t maximo = 0;

    uint32_t suma = 0;

    for (int i = 0; i < N; i++)
    {
        uint16_t x = datos[i];

        if (x < minimo)
            minimo = x;

        if (x > maximo)
            maximo = x;

        suma += x;
    }

    float media =
        (float)suma /
        (float)N;

    // =================================================
    // SALIDA PARA graficar.py
    // =================================================

    Serial.println();
    Serial.println(
        "----------- MEDICION -----------"
    );

    Serial.print("Fs objetivo: ");
    Serial.print(FS);
    Serial.println(" Hz");

    Serial.print("Fs real:     ");
    Serial.print(fsReal, 1);
    Serial.println(" Hz");

    Serial.print("Tiempo:      ");
    Serial.print(
        tiempo_s * 1000.0f,
        3
    );
    Serial.println(" ms");

    Serial.print("Chirp:       ");
    Serial.print(F0, 1);
    Serial.print(" -> ");
    Serial.print(F1, 1);
    Serial.println(" Hz");

    Serial.print("Amplitud:    ");
    Serial.println(A, 1);

    Serial.print("M:           ");
    Serial.print(M);
    Serial.println(" muestras");

    Serial.print("Duracion:    ");
    Serial.print(
        ((float)M / (float)FS) *
        1000.0f,
        3
    );
    Serial.println(" ms");

    Serial.println(
        "Salida:      DAC GPIO25"
    );

    Serial.print("Reposo DAC:  ");
    Serial.println(DAC_CENTRO);

    Serial.print("Min:         ");
    Serial.println(minimo);

    Serial.print("Max:         ");
    Serial.println(maximo);

    Serial.print("Rango:       ");
    Serial.println(
        maximo - minimo
    );

    Serial.print("Media ADC:   ");
    Serial.println(
        media,
        1
    );

    Serial.println(
        "--------------------------------"
    );

    // =================================================
    // 2048 MUESTRAS
    // =================================================

    for (int i = 0; i < N; i++)
    {
        Serial.println(datos[i]);
    }

    Serial.println(
        "------------- FIN --------------"
    );
}