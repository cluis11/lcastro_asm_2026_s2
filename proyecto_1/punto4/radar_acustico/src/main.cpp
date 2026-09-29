#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/adc.h"

// =====================================================
// CONFIGURACION GENERAL
// =====================================================

const uint32_t FS = 86000;
const int N = 2048;

// -----------------------------------------------------
// ADC / MICROFONO
// -----------------------------------------------------

const adc1_channel_t CANAL_ADC = ADC1_CHANNEL_6; // GPIO34
const i2s_port_t I2S_PORT = I2S_NUM_0;

// -----------------------------------------------------
// DAC / TRANSMISOR
// -----------------------------------------------------

const int PIN_DAC = 25;
const uint8_t DAC_CENTRO = 128;

// -----------------------------------------------------
// CHIRP
// -----------------------------------------------------

const int M = 172;
const float F0 = 4000.0f;
const float F1 = 10000.0f;
const float A = 20.0f;   // empezamos bajo para evitar saturacion/distorsion

uint8_t chirp[M];

// -----------------------------------------------------
// BUFFER ADC
// -----------------------------------------------------

uint16_t datos[N];

// =====================================================
// GENERAR CHIRP
// =====================================================

void generarChirp()
{
    float duracionBarrido = (float)(M - 1) / (float)FS;
    float k = (F1 - F0) / duracionBarrido;

    for (int n = 0; n < M; n++)
    {
        float t = (float)n / (float)FS;
        float fase = 2.0f * PI * (F0 * t + 0.5f * k * t * t);              // chirp lineal
        float ventana = 0.5f * (1.0f - cosf(2.0f * PI * (float)n / (float)(M - 1))); // ventana Hann
        float valor = (float)DAC_CENTRO + A * ventana * sinf(fase);
        int valorEntero = constrain((int)roundf(valor), 0, 255);
        chirp[n] = (uint8_t)valorEntero;
    }
}

// =====================================================
// CONFIGURAR I2S ADC
// =====================================================

void configurarI2S()
{
    i2s_config_t config = {};
    config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN);
    config.sample_rate = FS;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    config.communication_format = I2S_COMM_FORMAT_I2S_MSB;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 8;
    config.dma_buf_len = 256;
    config.use_apll = false;
    config.tx_desc_auto_clear = false;
    config.fixed_mclk = 0;

    esp_err_t err = i2s_driver_install(I2S_PORT, &config, 0, nullptr);
    if (err != ESP_OK)
    {
        Serial.print("Error i2s_driver_install: ");
        Serial.println(err);
        while (true) { delay(1000); }
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, CANAL_ADC);
    if (err != ESP_OK)
    {
        Serial.print("Error i2s_set_adc_mode: ");
        Serial.println(err);
        while (true) { delay(1000); }
    }

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(CANAL_ADC, ADC_ATTEN_DB_11);
}

// =====================================================
// VACIAR DMA
// =====================================================

void vaciarDMA()
{
    uint16_t buffer[256];
    size_t bytesLeidos = 0;

    for (int k = 0; k < 4; k++)
    {
        i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesLeidos, portMAX_DELAY);
    }
}

// =====================================================
// TRANSMITIR CHIRP POR DAC
// =====================================================
//
// A 86 kHz: 1/86000 = 11.63 us. micros() solo tiene resolucion
// entera en us, asi que este metodo NO es perfecto para la
// sincronizacion final, pero sirve para validar primero el
// funcionamiento.
//

void transmitirChirp()
{
    const float periodo_us = 1000000.0f / (float)FS;
    float siguiente = (float)micros();

    for (int i = 0; i < M; i++)
    {
        while ((float)((int32_t)(micros() - (uint32_t)siguiente)) < 0) {}
        dacWrite(PIN_DAC, chirp[i]);
        siguiente += periodo_us;
    }

    while ((float)((int32_t)(micros() - (uint32_t)siguiente)) < 0) {}
    dacWrite(PIN_DAC, DAC_CENTRO);
}

// =====================================================
// CAPTURAR
// =====================================================

void capturar()
{
    uint16_t bufferDMA[256];
    int indice = 0;
    size_t bytesLeidos = 0;

    dacWrite(PIN_DAC, DAC_CENTRO);
    i2s_adc_enable(I2S_PORT);
    vaciarDMA();

    uint32_t tInicio = micros();
    transmitirChirp();   // el DMA ya esta capturando en paralelo

    while (indice < N)
    {
        bytesLeidos = 0;
        esp_err_t err = i2s_read(I2S_PORT, bufferDMA, sizeof(bufferDMA), &bytesLeidos, portMAX_DELAY);
        if (err != ESP_OK) { continue; }

        int muestrasLeidas = bytesLeidos / sizeof(uint16_t);
        for (int i = 0; i < muestrasLeidas && indice < N; i++)
        {
            datos[indice] = bufferDMA[i] & 0x0FFF;
            indice++;
        }
    }

    uint32_t tFin = micros();
    i2s_adc_disable(I2S_PORT);
    dacWrite(PIN_DAC, DAC_CENTRO);

    // =================================================
    // ESTADISTICAS
    // =================================================

    uint16_t minimo = 4095, maximo = 0;
    uint32_t suma = 0;

    for (int i = 0; i < N; i++)
    {
        uint16_t x = datos[i];
        if (x < minimo) minimo = x;
        if (x > maximo) maximo = x;
        suma += x;
    }

    float media = (float)suma / (float)N;
    float tiempo_s = (float)(tFin - tInicio) / 1000000.0f;
    float fsMedida = (float)N / tiempo_s;

    // =================================================
    // RESULTADOS
    // =================================================

    Serial.println();
    Serial.println("----------- MEDICION -----------");
    Serial.print("Fs objetivo: "); Serial.print(FS); Serial.println(" Hz");
    Serial.print("Fs real:     "); Serial.print(fsMedida, 1); Serial.println(" Hz");
    Serial.print("Tiempo:      "); Serial.print(tiempo_s * 1000.0f, 3); Serial.println(" ms");
    Serial.print("Chirp:       "); Serial.print(F0, 1); Serial.print(" -> "); Serial.print(F1, 1); Serial.println(" Hz");
    Serial.print("Amplitud:    "); Serial.println(A, 1);
    Serial.print("M:           "); Serial.print(M); Serial.println(" muestras");
    Serial.print("Duracion:    "); Serial.print(((float)M / (float)FS) * 1000.0f, 3); Serial.println(" ms");
    Serial.println("Salida:      DAC GPIO25");
    Serial.println("Entrada:     I2S ADC GPIO34");
    Serial.print("Reposo DAC:  "); Serial.println(DAC_CENTRO);
    Serial.print("Min:         "); Serial.println(minimo);
    Serial.print("Max:         "); Serial.println(maximo);
    Serial.print("Rango:       "); Serial.println(maximo - minimo);
    Serial.print("Media ADC:   "); Serial.println(media, 1);
    Serial.println("--------------------------------");

    // =================================================
    // ENVIAR MUESTRAS
    // =================================================

    for (int i = 0; i < N; i++) { Serial.println(datos[i]); }
    Serial.println("------------- FIN --------------");
}

// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);
    delay(1000);

    generarChirp();
    dacWrite(PIN_DAC, DAC_CENTRO);
    configurarI2S();

    Serial.println();
    Serial.println("Radar listo.");
    Serial.println("ADC I2S = 86 kHz");
    Serial.println("Chirp = 4 kHz -> 10 kHz");
    Serial.println("M = 172");
    Serial.println("Duracion = 2 ms");
    Serial.println("A = 5");
    Serial.println("Enviar x para medir.");
}

// =====================================================
// LOOP
// =====================================================

void loop()
{
    if (!Serial.available()) { return; }
    while (Serial.available()) { Serial.read(); }
    capturar();
}