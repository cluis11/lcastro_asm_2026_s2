#include <Arduino.h>
#include "driver/i2s.h"
#include "driver/adc.h"
#include <math.h>

// =====================================================
// CONFIGURACION GENERAL
// =====================================================

const uint32_t FS = 86000;
const int N = 2048;

// =====================================================
// ADC / MICROFONO
// =====================================================

const adc1_channel_t CANAL_ADC = ADC1_CHANNEL_6; // GPIO34
const i2s_port_t I2S_PORT = I2S_NUM_0;

// =====================================================
// DAC / TRANSMISOR
// =====================================================

const int PIN_DAC = 25;
const uint8_t DAC_CENTRO = 128;

// =====================================================
// CHIRP
// =====================================================

const int M = 172;
const float F0 = 4000.0f;
const float F1 = 10000.0f;
const float A = 20.0f;

uint8_t chirp[M];

// =====================================================
// FILTRO FIR PASA BANDAS
// =====================================================

const int FIR_TAPS = 129;
const float FIR_F_MIN = 3500.0f;
const float FIR_F_MAX = 11000.0f;

float fir[FIR_TAPS];

// =====================================================
// CALIBRACION DE RUIDO
// =====================================================

const int NUM_CAPTURAS_RUIDO = 5;

bool ruidoCalibrado = false;

float ruidoRmsPromedio = 0.0f;
float ruidoRmsMaximo = 0.0f;
float ruidoPicoMaximo = 0.0f;

// =====================================================
// BUFFERS
// =====================================================

uint16_t datos[N];
float centrado[N];
float filtrado[N];

// =====================================================
// SINC
// =====================================================

float sincF(float x) {
    if (fabsf(x) < 1e-8f) {
        return 1.0f;
    }

    return sinf(PI * x) / (PI * x);
}

// =====================================================
// GENERAR FILTRO FIR
// =====================================================

void generarFiltroFIR() {
    const int centro = (FIR_TAPS - 1) / 2;

    for (int i = 0; i < FIR_TAPS; i++) {
        float n = (float)(i - centro);

        float hMax = 2.0f * FIR_F_MAX / FS *
                     sincF(2.0f * FIR_F_MAX * n / FS);

        float hMin = 2.0f * FIR_F_MIN / FS *
                     sincF(2.0f * FIR_F_MIN * n / FS);

        float h = hMax - hMin;

        float ventana = 0.54f -
                        0.46f * cosf(
                            2.0f * PI * i /
                            (FIR_TAPS - 1)
                        );

        fir[i] = h * ventana;
    }
}

// =====================================================
// GENERAR CHIRP
// =====================================================

void generarChirp() {
    float duracionBarrido = (float)(M - 1) / FS;
    float k = (F1 - F0) / duracionBarrido;

    for (int n = 0; n < M; n++) {
        float t = (float)n / FS;

        float fase = 2.0f * PI *
                     (F0 * t + 0.5f * k * t * t);

        float ventana = 0.5f *
                        (1.0f - cosf(
                            2.0f * PI * n /
                            (M - 1)
                        ));

        float valor = DAC_CENTRO +
                      A * ventana * sinf(fase);

        chirp[n] = (uint8_t)constrain(
            (int)roundf(valor),
            0,
            255
        );
    }
}

// =====================================================
// CONFIGURAR I2S ADC
// =====================================================

void configurarI2S() {
    i2s_config_t config = {};

    config.mode = (i2s_mode_t)(
        I2S_MODE_MASTER |
        I2S_MODE_RX |
        I2S_MODE_ADC_BUILT_IN
    );

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

    esp_err_t err = i2s_driver_install(
        I2S_PORT,
        &config,
        0,
        nullptr
    );

    if (err != ESP_OK) {
        Serial.print("Error i2s_driver_install: ");
        Serial.println(err);

        while (true) {
            delay(1000);
        }
    }

    err = i2s_set_adc_mode(
        ADC_UNIT_1,
        CANAL_ADC
    );

    if (err != ESP_OK) {
        Serial.print("Error i2s_set_adc_mode: ");
        Serial.println(err);

        while (true) {
            delay(1000);
        }
    }

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(CANAL_ADC, ADC_ATTEN_DB_11);
}

// =====================================================
// VACIAR DMA
// =====================================================

void vaciarDMA() {
    uint16_t buffer[256];
    size_t bytesLeidos = 0;

    for (int k = 0; k < 4; k++) {
        i2s_read(
            I2S_PORT,
            buffer,
            sizeof(buffer),
            &bytesLeidos,
            portMAX_DELAY
        );
    }
}

// =====================================================
// TRANSMITIR CHIRP
// =====================================================

void transmitirChirp() {
    const float periodoUs = 1000000.0f / FS;
    float siguiente = (float)micros();

    for (int i = 0; i < M; i++) {
        while ((int32_t)(micros() - (uint32_t)siguiente) < 0) {
        }

        dacWrite(PIN_DAC, chirp[i]);

        siguiente += periodoUs;
    }

    while ((int32_t)(micros() - (uint32_t)siguiente) < 0) {
    }

    dacWrite(PIN_DAC, DAC_CENTRO);
}

// =====================================================
// CAPTURAR ADC
// =====================================================

float capturarADC(bool emitirChirp) {
    uint16_t bufferDMA[256];
    size_t bytesLeidos = 0;
    int indice = 0;

    dacWrite(PIN_DAC, DAC_CENTRO);

    i2s_adc_enable(I2S_PORT);
    vaciarDMA();

    uint32_t tInicio = micros();

    if (emitirChirp) {
        transmitirChirp();
    }

    while (indice < N) {
        bytesLeidos = 0;

        esp_err_t err = i2s_read(
            I2S_PORT,
            bufferDMA,
            sizeof(bufferDMA),
            &bytesLeidos,
            portMAX_DELAY
        );

        if (err != ESP_OK) {
            continue;
        }

        int muestrasLeidas = bytesLeidos / sizeof(uint16_t);

        for (int i = 0; i < muestrasLeidas && indice < N; i++) {
            datos[indice] = bufferDMA[i] & 0x0FFF;
            indice++;
        }
    }

    uint32_t tFin = micros();

    i2s_adc_disable(I2S_PORT);
    dacWrite(PIN_DAC, DAC_CENTRO);

    return (float)(tFin - tInicio) / 1000.0f;
}

// =====================================================
// QUITAR DC
// =====================================================

float quitarDC() {
    uint32_t suma = 0;

    for (int i = 0; i < N; i++) {
        suma += datos[i];
    }

    float media = (float)suma / N;

    for (int i = 0; i < N; i++) {
        centrado[i] = (float)datos[i] - media;
    }

    return media;
}

// =====================================================
// FILTRO FIR
// =====================================================

void aplicarFiltroFIR() {
    for (int n = 0; n < N; n++) {
        float suma = 0.0f;

        for (int k = 0; k < FIR_TAPS; k++) {
            int indice = n - k;

            if (indice >= 0) {
                suma += fir[k] * centrado[indice];
            }
        }

        filtrado[n] = suma;
    }
}

// =====================================================
// RMS
// =====================================================

float calcularRMS(const float *senal) {
    double suma = 0.0;

    for (int i = 0; i < N; i++) {
        suma += (double)senal[i] * senal[i];
    }

    return sqrt(suma / N);
}

// =====================================================
// PICO ABSOLUTO
// =====================================================

float calcularPicoAbsoluto(const float *senal) {
    float maximo = 0.0f;

    for (int i = 0; i < N; i++) {
        float valor = fabsf(senal[i]);

        if (valor > maximo) {
            maximo = valor;
        }
    }

    return maximo;
}

// =====================================================
// ESTADISTICAS ADC
// =====================================================

void calcularEstadisticasADC(
    uint16_t &minimo,
    uint16_t &maximo,
    float &media
) {
    minimo = 4095;
    maximo = 0;

    uint32_t suma = 0;

    for (int i = 0; i < N; i++) {
        uint16_t x = datos[i];

        if (x < minimo) {
            minimo = x;
        }

        if (x > maximo) {
            maximo = x;
        }

        suma += x;
    }

    media = (float)suma / N;
}

// =====================================================
// CALIBRAR RUIDO
// =====================================================

void calibrarRuido() {
    Serial.println();
    Serial.println("================================");
    Serial.println(" CALIBRACION DE RUIDO");
    Serial.println("================================");
    Serial.println("No se emitira chirp.");
    Serial.print("Capturas: ");
    Serial.println(NUM_CAPTURAS_RUIDO);
    Serial.println();

    float sumaRms = 0.0f;
    float maxRms = 0.0f;
    float maxPico = 0.0f;

    for (int captura = 0; captura < NUM_CAPTURAS_RUIDO; captura++) {
        float tiempoCaptura = capturarADC(false);

        quitarDC();
        aplicarFiltroFIR();

        float rms = calcularRMS(filtrado);
        float pico = calcularPicoAbsoluto(filtrado);

        sumaRms += rms;

        if (rms > maxRms) {
            maxRms = rms;
        }

        if (pico > maxPico) {
            maxPico = pico;
        }

        Serial.print("Captura ");
        Serial.print(captura + 1);
        Serial.print("/");
        Serial.print(NUM_CAPTURAS_RUIDO);

        Serial.print(" | RMS: ");
        Serial.print(rms, 2);

        Serial.print(" | Pico: ");
        Serial.print(pico, 2);

        Serial.print(" | Tiempo: ");
        Serial.print(tiempoCaptura, 3);
        Serial.println(" ms");

        delay(50);
    }

    ruidoRmsPromedio = sumaRms / NUM_CAPTURAS_RUIDO;
    ruidoRmsMaximo = maxRms;
    ruidoPicoMaximo = maxPico;

    ruidoCalibrado = true;

    Serial.println();
    Serial.println("----------- RESULTADO -----------");

    Serial.print("RMS promedio: ");
    Serial.println(ruidoRmsPromedio, 2);

    Serial.print("RMS maximo:   ");
    Serial.println(ruidoRmsMaximo, 2);

    Serial.print("Pico maximo:  ");
    Serial.println(ruidoPicoMaximo, 2);

    Serial.println();
    Serial.println("Calibracion guardada en RAM.");
    Serial.println("--------------------------------");
}

// =====================================================
// MEDICION NORMAL
// =====================================================

void medir() {
    float tiempoCaptura = capturarADC(true);

    uint16_t minimo;
    uint16_t maximo;
    float media;

    calcularEstadisticasADC(minimo, maximo, media);

    for (int i = 0; i < N; i++) {
        centrado[i] = (float)datos[i] - media;
    }

    uint32_t tFiltroInicio = micros();

    aplicarFiltroFIR();

    uint32_t tFiltroFin = micros();

    float rmsCruda = calcularRMS(centrado);
    float rmsFiltrada = calcularRMS(filtrado);
    float picoFiltrado = calcularPicoAbsoluto(filtrado);

    float tiempoFiltro =
        (float)(tFiltroFin - tFiltroInicio) / 1000.0f;

    float fsReportada =
        (float)N / (tiempoCaptura / 1000.0f);

    Serial.println();
    Serial.println("----------- MEDICION -----------");

    Serial.print("Fs objetivo: ");
    Serial.print(FS);
    Serial.println(" Hz");

    Serial.print("Fs reportada: ");
    Serial.print(fsReportada, 1);
    Serial.println(" Hz");

    Serial.print("Tiempo captura: ");
    Serial.print(tiempoCaptura, 3);
    Serial.println(" ms");

    Serial.print("Chirp: ");
    Serial.print(F0, 1);
    Serial.print(" -> ");
    Serial.print(F1, 1);
    Serial.println(" Hz");

    Serial.print("Amplitud: ");
    Serial.println(A, 1);

    Serial.print("M: ");
    Serial.print(M);
    Serial.println(" muestras");

    Serial.print("Duracion chirp: ");
    Serial.print((float)M / FS * 1000.0f, 3);
    Serial.println(" ms");

    Serial.print("ADC min: ");
    Serial.println(minimo);

    Serial.print("ADC max: ");
    Serial.println(maximo);

    Serial.print("ADC rango: ");
    Serial.println(maximo - minimo);

    Serial.print("ADC media: ");
    Serial.println(media, 2);

    Serial.println();
    Serial.println("----------- FILTRO FIR ----------");

    Serial.print("Banda: ");
    Serial.print(FIR_F_MIN / 1000.0f, 1);
    Serial.print(" -> ");
    Serial.print(FIR_F_MAX / 1000.0f, 1);
    Serial.println(" kHz");

    Serial.print("Taps: ");
    Serial.println(FIR_TAPS);

    Serial.print("RMS cruda: ");
    Serial.println(rmsCruda, 2);

    Serial.print("RMS filtrada: ");
    Serial.println(rmsFiltrada, 2);

    Serial.print("Pico filtrado: ");
    Serial.println(picoFiltrado, 2);

    Serial.print("Tiempo FIR: ");
    Serial.print(tiempoFiltro, 3);
    Serial.println(" ms");

    Serial.print("Ruido calibrado: ");
    Serial.println(ruidoCalibrado ? "SI" : "NO");

    if (ruidoCalibrado) {
        Serial.print("RMS ruido: ");
        Serial.println(ruidoRmsPromedio, 2);

        Serial.print("Pico ruido: ");
        Serial.println(ruidoPicoMaximo, 2);
    }

    Serial.println("--------------------------------");

    Serial.println("----------- CRUDOS --------------");

    for (int i = 0; i < N; i++) {
        Serial.println(datos[i]);
    }

    Serial.println("----------- FILTRADOS -----------");

    for (int i = 0; i < N; i++) {
        Serial.println(filtrado[i], 6);
    }

    Serial.println("------------- FIN ---------------");
}

// =====================================================
// MOSTRAR ESTADO
// =====================================================

void mostrarEstado() {
    Serial.println();
    Serial.println("----------- ESTADO --------------");

    Serial.print("Ruido calibrado: ");
    Serial.println(ruidoCalibrado ? "SI" : "NO");

    if (ruidoCalibrado) {
        Serial.print("RMS promedio ruido: ");
        Serial.println(ruidoRmsPromedio, 2);

        Serial.print("RMS maximo ruido: ");
        Serial.println(ruidoRmsMaximo, 2);

        Serial.print("Pico maximo ruido: ");
        Serial.println(ruidoPicoMaximo, 2);
    }

    Serial.println("--------------------------------");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    generarChirp();
    generarFiltroFIR();

    dacWrite(PIN_DAC, DAC_CENTRO);

    configurarI2S();

    Serial.println();
    Serial.println("Radar listo.");
    Serial.println("Fs = 86 kHz");
    Serial.println("Chirp = 4-10 kHz");
    Serial.println("Duracion = 2 ms");
    Serial.println("A = 20");
    Serial.println("FIR = 3.5-11 kHz");
    Serial.println();
    Serial.println("Comandos:");
    Serial.println("r = calibrar ruido");
    Serial.println("x = medir con chirp");
    Serial.println("s = mostrar estado");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
    if (!Serial.available()) {
        return;
    }

    char comando = Serial.read();

    while (Serial.available()) {
        Serial.read();
    }

    if (comando == 'r') {
        calibrarRuido();
    }
    else if (comando == 'x') {
        medir();
    }
    else if (comando == 's') {
        mostrarEstado();
    }
}