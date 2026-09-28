#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/adc.h"

// =====================================================
// CONFIGURACION
// =====================================================

const uint32_t FS = 32000;
const int N = 2048;

// GPIO34 = ADC1_CHANNEL_6
const adc1_channel_t CANAL_ADC = ADC1_CHANNEL_6;

// I2S
const i2s_port_t I2S_PORT = I2S_NUM_0;

// Buffer final
uint16_t datos[N];

// =====================================================
// CONFIGURAR I2S + ADC
// =====================================================

void configurarI2S()
{
    i2s_config_t config = {};

    config.mode = (i2s_mode_t)(
        I2S_MODE_MASTER |
        I2S_MODE_RX |
        I2S_MODE_ADC_BUILT_IN
    );

    config.sample_rate = FS;

    config.bits_per_sample =
        I2S_BITS_PER_SAMPLE_16BIT;

    config.channel_format =
        I2S_CHANNEL_FMT_ONLY_LEFT;

    config.communication_format =
        I2S_COMM_FORMAT_I2S_MSB;

    config.intr_alloc_flags =
        ESP_INTR_FLAG_LEVEL1;

    config.dma_buf_count = 8;

    config.dma_buf_len = 256;

    config.use_apll = false;

    config.tx_desc_auto_clear = false;

    config.fixed_mclk = 0;


    // Instalar driver
    esp_err_t err = i2s_driver_install(
        I2S_PORT,
        &config,
        0,
        nullptr
    );

    if (err != ESP_OK)
    {
        Serial.print(
            "Error i2s_driver_install: "
        );

        Serial.println(err);

        while (true) {
            delay(1000);
        }
    }


    // GPIO34 = ADC1 canal 6
    err = i2s_set_adc_mode(
        ADC_UNIT_1,
        CANAL_ADC
    );

    if (err != ESP_OK)
    {
        Serial.print(
            "Error i2s_set_adc_mode: "
        );

        Serial.println(err);

        while (true) {
            delay(1000);
        }
    }


    // Atenuacion similar a la prueba anterior
    adc1_config_channel_atten(
        CANAL_ADC,
        ADC_ATTEN_DB_11
    );


    // Resolucion ADC
    adc1_config_width(
        ADC_WIDTH_BIT_12
    );
}


// =====================================================
// CAPTURA
// =====================================================

void capturarI2S()
{
    uint16_t bufferDMA[256];

    int indice = 0;

    size_t bytesLeidos = 0;


    // Activar ADC mediante I2S
    i2s_adc_enable(
        I2S_PORT
    );


    // -------------------------------------------------
    // Vaciar unas muestras antiguas del DMA
    // -------------------------------------------------

    for (int k = 0; k < 4; k++)
    {
        i2s_read(
            I2S_PORT,
            bufferDMA,
            sizeof(bufferDMA),
            &bytesLeidos,
            portMAX_DELAY
        );
    }


    // -------------------------------------------------
    // Medir tiempo de la captura
    // -------------------------------------------------

    uint32_t tInicio =
        micros();


    while (indice < N)
    {
        bytesLeidos = 0;

        esp_err_t err = i2s_read(
            I2S_PORT,
            bufferDMA,
            sizeof(bufferDMA),
            &bytesLeidos,
            portMAX_DELAY
        );

        if (err != ESP_OK)
        {
            continue;
        }


        int muestrasLeidas =
            bytesLeidos /
            sizeof(uint16_t);


        for (
            int i = 0;
            i < muestrasLeidas &&
            indice < N;
            i++
        )
        {
            uint16_t raw =
                bufferDMA[i];

            // ADC de 12 bits
            datos[indice] =
                raw & 0x0FFF;

            indice++;
        }
    }


    uint32_t tFin =
        micros();


    i2s_adc_disable(
        I2S_PORT
    );


    // =================================================
    // ESTADISTICAS
    // =================================================

    uint16_t minimo = 4095;
    uint16_t maximo = 0;

    uint32_t suma = 0;


    for (int i = 0; i < N; i++)
    {
        uint16_t x =
            datos[i];

        if (x < minimo)
            minimo = x;

        if (x > maximo)
            maximo = x;

        suma += x;
    }


    float media =
        (float)suma /
        (float)N;


    float tiempo_s =
        (float)(tFin - tInicio) /
        1000000.0f;


    float fsMedida =
        (float)N /
        tiempo_s;


    // =================================================
    // SALIDA
    // =================================================

    Serial.println();

    Serial.println(
        "----------- MEDICION -----------"
    );

    Serial.print(
        "Fs objetivo: "
    );

    Serial.print(FS);

    Serial.println(
        " Hz"
    );


    Serial.print(
        "Fs real:     "
    );

    Serial.print(
        fsMedida,
        1
    );

    Serial.println(
        " Hz"
    );


    Serial.print(
        "Tiempo:      "
    );

    Serial.print(
        tiempo_s * 1000.0f,
        3
    );

    Serial.println(
        " ms"
    );


    Serial.println(
        "Modo:        I2S ADC SIN CHIRP"
    );


    Serial.println(
        "ADC:         GPIO34 / ADC1_CH6"
    );


    Serial.print(
        "Min:         "
    );

    Serial.println(
        minimo
    );


    Serial.print(
        "Max:         "
    );

    Serial.println(
        maximo
    );


    Serial.print(
        "Rango:       "
    );

    Serial.println(
        maximo - minimo
    );


    Serial.print(
        "Media ADC:   "
    );

    Serial.println(
        media,
        1
    );


    Serial.println(
        "--------------------------------"
    );


    // =================================================
    // MUESTRAS
    // =================================================

    for (int i = 0; i < N; i++)
    {
        Serial.println(
            datos[i]
        );
    }


    Serial.println(
        "------------- FIN --------------"
    );
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    configurarI2S();

    Serial.println();
    Serial.println(
        "I2S ADC listo."
    );

    Serial.println(
        "Fs = 32000 Hz"
    );

    Serial.println(
        "GPIO34 = ADC1_CH6"
    );

    Serial.println(
        "Enviar x para capturar."
    );
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
    if (!Serial.available())
    {
        return;
    }


    while (Serial.available())
    {
        Serial.read();
    }


    capturarI2S();
}