#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "driver/i2s.h"
#include "driver/adc.h"
#include <math.h>

// =====================================================
// CONFIGURACION GENERAL
// =====================================================

const uint32_t FS = 86000;
const int N = 2048;

// =====================================================
// LCD
// =====================================================

const int LCD_SDA = 21;
const int LCD_SCL = 22;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// =====================================================
// BOTONES
// =====================================================

const int PIN_BOTON_RUIDO = 27;
const int PIN_BOTON_BASELINE = 32;
const int PIN_BOTON_MEDIR = 33;

const unsigned long DEBOUNCE_MS = 50;

bool estadoAnteriorRuido = HIGH;
bool estadoAnteriorBaseline = HIGH;
bool estadoAnteriorMedir = HIGH;

unsigned long ultimoCambioRuido = 0;
unsigned long ultimoCambioBaseline = 0;
unsigned long ultimoCambioMedir = 0;

// =====================================================
// GEOMETRIA / DISTANCIA
// =====================================================

const float VELOCIDAD_SONIDO = 343.0f;
const float SEPARACION_TX_RX = 0.10f;
const int OFFSET_CORRELACION = 221;

// =====================================================
// ADC / MICROFONO
// =====================================================

const adc1_channel_t CANAL_ADC = ADC1_CHANNEL_6;
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
const float A = 10.0f;

uint8_t chirp[M];
float chirpReferencia[M];

// =====================================================
// FIR
// =====================================================

const int FIR_TAPS = 129;
const float FIR_F_MIN = 3500.0f;
const float FIR_F_MAX = 11000.0f;
const int FIR_DESCARTE_INICIAL = FIR_TAPS - 1;

float fir[FIR_TAPS];

// =====================================================
// CALIBRACION DE RUIDO
// =====================================================

const int NUM_CAPTURAS_RUIDO = 5;
const int NUM_CAPTURAS_CALENTAMIENTO = 1;

bool ruidoCalibrado = false;

float ruidoRmsPromedio = 0.0f;
float ruidoRmsMaximo = 0.0f;
float ruidoPicoMaximo = 0.0f;

// =====================================================
// BASELINE
// =====================================================

const int NUM_CAPTURAS_BASELINE = 5;

bool baselineCalibrado = false;

float baseline[N];

float baselineRms = 0.0f;
float baselinePico = 0.0f;

// =====================================================
// BUFFERS
// =====================================================

uint16_t datos[N];

float centrado[N];
float filtrado[N];
float residual[N];

// =====================================================
// RESULTADOS
// =====================================================

struct ResultadoCorrelacion {
    int indice;
    float valor;
    float tiempoMs;
};

struct ResultadoDistancia {
    bool valida;
    int indiceCorregido;
    float tiempoVueloMs;
    float recorridoMetros;
    float distanciaMetros;
};

// =====================================================
// LCD
// =====================================================

void mostrarDistanciaLCD(float distanciaCm) {
    char texto[17];

    snprintf(texto, sizeof(texto), "%.2f cm", distanciaCm);

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Distancia:");
    lcd.setCursor(0, 1);
    lcd.print(texto);
}

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
// GENERAR FIR
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
        float fase = 2.0f * PI * (F0 * t + 0.5f * k * t * t);
        float ventana = 0.5f * (1.0f - cosf(2.0f * PI * n / (M - 1)));

        float muestra = ventana * sinf(fase);

        chirpReferencia[n] = muestra;

        float valor = DAC_CENTRO + A * muestra;

        chirp[n] = (uint8_t)constrain((int)roundf(valor), 0, 255);
    }
}

// =====================================================
// CONFIGURAR I2S
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

    esp_err_t err = i2s_driver_install(I2S_PORT, &config, 0, nullptr);

    if (err != ESP_OK) {
        Serial.print("Error i2s_driver_install: ");
        Serial.println(err);

        while (true) {
            delay(1000);
        }
    }

    err = i2s_set_adc_mode(ADC_UNIT_1, CANAL_ADC);

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
        i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesLeidos, portMAX_DELAY);
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
// CAPTURA ADC
// =====================================================

float capturarADCActivo(bool emitirChirp) {
    size_t bytesLeidos = 0;

    vaciarDMA();

    uint32_t tInicio = micros();

    if (emitirChirp) {
        transmitirChirp();
    }

    esp_err_t err = i2s_read(
        I2S_PORT,
        datos,
        sizeof(datos),
        &bytesLeidos,
        portMAX_DELAY
    );

    uint32_t tFin = micros();

    if (err != ESP_OK) {
        Serial.print("Error i2s_read: ");
        Serial.println(err);
        return -1.0f;
    }

    int muestrasLeidas = bytesLeidos / sizeof(uint16_t);

    for (int i = 0; i < muestrasLeidas; i++) {
        datos[i] &= 0x0FFF;
    }

    for (int i = muestrasLeidas; i < N; i++) {
        datos[i] = 0;
    }

    return (float)(tFin - tInicio) / 1000.0f;
}

float capturarADC(bool emitirChirp) {
    i2s_adc_enable(I2S_PORT);

    float tiempo = capturarADCActivo(emitirChirp);

    i2s_adc_disable(I2S_PORT);

    return tiempo;
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
// FIR
// =====================================================

void aplicarFiltroFIR() {
    const int centro = (FIR_TAPS - 1) / 2;

    for (int n = 0; n < N; n++) {
        float suma = 0.0f;

        for (int k = 0; k < centro; k++) {
            int indiceA = n - k;
            int indiceB = n - (FIR_TAPS - 1 - k);

            if (indiceA >= 0) {
                suma += fir[k] * centrado[indiceA];
            }

            if (indiceB >= 0) {
                suma += fir[k] * centrado[indiceB];
            }
        }

        int indiceCentro = n - centro;

        if (indiceCentro >= 0) {
            suma += fir[centro] * centrado[indiceCentro];
        }

        filtrado[n] = suma;
    }
}

// =====================================================
// RMS
// =====================================================

float calcularRMS(const float *senal, int inicio = 0) {
    if (inicio < 0) {
        inicio = 0;
    }

    if (inicio >= N) {
        return 0.0f;
    }

    double suma = 0.0;
    int cantidad = N - inicio;

    for (int i = inicio; i < N; i++) {
        suma += (double)senal[i] * senal[i];
    }

    return sqrt(suma / cantidad);
}

// =====================================================
// PICO
// =====================================================

float calcularPicoAbsoluto(const float *senal, int inicio = 0) {
    if (inicio < 0) {
        inicio = 0;
    }

    if (inicio >= N) {
        return 0.0f;
    }

    float maximo = 0.0f;

    for (int i = inicio; i < N; i++) {
        float valor = fabsf(senal[i]);

        if (valor > maximo) {
            maximo = valor;
        }
    }

    return maximo;
}

// =====================================================
// CORRELACION
// =====================================================

ResultadoCorrelacion correlacionarResidual() {
    ResultadoCorrelacion resultado;

    resultado.indice = -1;
    resultado.valor = 0.0f;
    resultado.tiempoMs = 0.0f;

    float maxAbsoluto = 0.0f;
    int ultimoIndice = N - M;

    for (int desplazamiento = FIR_DESCARTE_INICIAL;
         desplazamiento <= ultimoIndice;
         desplazamiento++) {

        float suma = 0.0f;

        for (int k = 0; k < M; k++) {
            suma += residual[desplazamiento + k] * chirpReferencia[k];
        }

        float absoluto = fabsf(suma);

        if (absoluto > maxAbsoluto) {
            maxAbsoluto = absoluto;
            resultado.indice = desplazamiento;
            resultado.valor = suma;
        }
    }

    if (resultado.indice >= 0) {
        resultado.tiempoMs = 1000.0f * resultado.indice / FS;
    }

    return resultado;
}

// =====================================================
// DISTANCIA
// =====================================================

ResultadoDistancia calcularDistancia(int indiceCorrelacion) {
    ResultadoDistancia resultado;

    resultado.valida = false;
    resultado.indiceCorregido = indiceCorrelacion - OFFSET_CORRELACION;
    resultado.tiempoVueloMs = 0.0f;
    resultado.recorridoMetros = 0.0f;
    resultado.distanciaMetros = 0.0f;

    if (resultado.indiceCorregido <= 0) {
        return resultado;
    }

    float tiempoVuelo = (float)resultado.indiceCorregido / FS;
    float recorrido = tiempoVuelo * VELOCIDAD_SONIDO;

    float mitadRecorrido = recorrido * 0.5f;
    float mitadSeparacion = SEPARACION_TX_RX * 0.5f;

    float argumento =
        mitadRecorrido * mitadRecorrido -
        mitadSeparacion * mitadSeparacion;

    if (argumento <= 0.0f) {
        return resultado;
    }

    resultado.tiempoVueloMs = tiempoVuelo * 1000.0f;
    resultado.recorridoMetros = recorrido;
    resultado.distanciaMetros = sqrtf(argumento);
    resultado.valida = true;

    return resultado;
}

// =====================================================
// CALIBRAR RUIDO
// =====================================================

void calibrarRuido() {
    Serial.println();
    Serial.println("================================");
    Serial.println(" CALIBRACION DE RUIDO");
    Serial.println("================================");

    for (int i = 0; i < NUM_CAPTURAS_CALENTAMIENTO; i++) {
        capturarADC(false);
        quitarDC();
        aplicarFiltroFIR();
    }

    float sumaRms = 0.0f;
    float maxRms = 0.0f;
    float maxPico = 0.0f;

    for (int captura = 0; captura < NUM_CAPTURAS_RUIDO; captura++) {
        capturarADC(false);

        quitarDC();
        aplicarFiltroFIR();

        float rms = calcularRMS(filtrado, FIR_DESCARTE_INICIAL);
        float pico = calcularPicoAbsoluto(filtrado, FIR_DESCARTE_INICIAL);

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
        Serial.println(pico, 2);

        delay(50);
    }

    ruidoRmsPromedio = sumaRms / NUM_CAPTURAS_RUIDO;
    ruidoRmsMaximo = maxRms;
    ruidoPicoMaximo = maxPico;

    ruidoCalibrado = true;

    Serial.println();
    Serial.print("RMS promedio: ");
    Serial.println(ruidoRmsPromedio, 2);

    Serial.print("RMS maximo: ");
    Serial.println(ruidoRmsMaximo, 2);

    Serial.print("Pico maximo: ");
    Serial.println(ruidoPicoMaximo, 2);
}

// =====================================================
// CALIBRAR BASELINE
// =====================================================

void calibrarBaseline() {
    Serial.println();
    Serial.println("================================");
    Serial.println(" CALIBRACION BASELINE");
    Serial.println("================================");
    Serial.println("Sin objeto objetivo enfrente.");

    for (int i = 0; i < N; i++) {
        baseline[i] = 0.0f;
    }

    capturarADC(true);
    quitarDC();
    aplicarFiltroFIR();

    for (int captura = 0; captura < NUM_CAPTURAS_BASELINE; captura++) {
        capturarADC(true);

        quitarDC();
        aplicarFiltroFIR();

        float rms = calcularRMS(filtrado, FIR_DESCARTE_INICIAL);
        float pico = calcularPicoAbsoluto(filtrado, FIR_DESCARTE_INICIAL);

        for (int i = 0; i < N; i++) {
            baseline[i] += filtrado[i];
        }

        Serial.print("Captura ");
        Serial.print(captura + 1);
        Serial.print("/");
        Serial.print(NUM_CAPTURAS_BASELINE);
        Serial.print(" | RMS: ");
        Serial.print(rms, 2);
        Serial.print(" | Pico: ");
        Serial.println(pico, 2);

        delay(100);
    }

    for (int i = 0; i < N; i++) {
        baseline[i] /= NUM_CAPTURAS_BASELINE;
    }

    baselineRms = calcularRMS(baseline, FIR_DESCARTE_INICIAL);
    baselinePico = calcularPicoAbsoluto(baseline, FIR_DESCARTE_INICIAL);

    baselineCalibrado = true;

    Serial.println();
    Serial.print("RMS baseline: ");
    Serial.println(baselineRms, 2);

    Serial.print("Pico baseline: ");
    Serial.println(baselinePico, 2);

    Serial.println();
    Serial.println("Baseline listo. Puede medir con 'x'.");
}

// =====================================================
// MEDIR
// =====================================================

void medir() {
    if (!baselineCalibrado) {
        Serial.println();
        Serial.println("ERROR: primero calibre baseline con 'b'.");
        return;
    }

    float tiempoCaptura = capturarADC(true);
    float media = quitarDC();

    uint32_t tFiltroInicio = micros();
    aplicarFiltroFIR();
    uint32_t tFiltroFin = micros();

    for (int i = 0; i < N; i++) {
        residual[i] = filtrado[i] - baseline[i];
    }

    uint32_t tCorrelacionInicio = micros();
    ResultadoCorrelacion corr = correlacionarResidual();
    uint32_t tCorrelacionFin = micros();

    ResultadoDistancia distancia = calcularDistancia(corr.indice);

    float rmsFiltrada = calcularRMS(filtrado, FIR_DESCARTE_INICIAL);
    float picoFiltrado = calcularPicoAbsoluto(filtrado, FIR_DESCARTE_INICIAL);

    float rmsResidual = calcularRMS(residual, FIR_DESCARTE_INICIAL);
    float picoResidual = calcularPicoAbsoluto(residual, FIR_DESCARTE_INICIAL);

    float tiempoFiltro =
        (float)(tFiltroFin - tFiltroInicio) / 1000.0f;

    float tiempoCorrelacion =
        (float)(tCorrelacionFin - tCorrelacionInicio) / 1000.0f;

    float relacionRmsRuido = 0.0f;
    float relacionPicoRuido = 0.0f;

    if (ruidoCalibrado && ruidoRmsPromedio > 0.0f) {
        relacionRmsRuido = rmsResidual / ruidoRmsPromedio;
    }

    if (ruidoCalibrado && ruidoPicoMaximo > 0.0f) {
        relacionPicoRuido = picoResidual / ruidoPicoMaximo;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println(" MEDICION");
    Serial.println("================================");

    Serial.print("Tiempo captura: ");
    Serial.print(tiempoCaptura, 3);
    Serial.println(" ms");

    Serial.print("Media ADC: ");
    Serial.println(media, 2);

    Serial.print("Tiempo FIR: ");
    Serial.print(tiempoFiltro, 3);
    Serial.println(" ms");

    Serial.print("Tiempo correlacion: ");
    Serial.print(tiempoCorrelacion, 3);
    Serial.println(" ms");

    Serial.println();
    Serial.println("----------- SENAL ---------------");

    Serial.print("RMS filtrada: ");
    Serial.println(rmsFiltrada, 2);

    Serial.print("Pico filtrado: ");
    Serial.println(picoFiltrado, 2);

    Serial.print("RMS residual: ");
    Serial.println(rmsResidual, 2);

    Serial.print("Pico residual: ");
    Serial.println(picoResidual, 2);

    if (ruidoCalibrado) {
        Serial.print("Residual / ruido RMS: ");
        Serial.println(relacionRmsRuido, 2);

        Serial.print("Residual / ruido pico: ");
        Serial.println(relacionPicoRuido, 2);
    }

    Serial.println();
    Serial.println("-------- CORRELACION ------------");

    Serial.print("Pico correlacion: ");
    Serial.println(corr.valor, 2);

    Serial.print("Indice correlacion: ");
    Serial.println(corr.indice);

    Serial.print("Tiempo correlacion: ");
    Serial.print(corr.tiempoMs, 3);
    Serial.println(" ms");

    Serial.println();
    Serial.println("================================");
    Serial.println(" RESULTADO DISTANCIA");
    Serial.println("================================");

    if (distancia.valida) {
        float distanciaCm = distancia.distanciaMetros * 100.0f;

        Serial.print("Indice corregido: ");
        Serial.println(distancia.indiceCorregido);

        Serial.print("Tiempo vuelo: ");
        Serial.print(distancia.tiempoVueloMs, 3);
        Serial.println(" ms");

        Serial.print("Recorrido total: ");
        Serial.print(distancia.recorridoMetros * 100.0f, 2);
        Serial.println(" cm");

        Serial.println();
        Serial.print(">>> DISTANCIA: ");
        Serial.print(distanciaCm, 2);
        Serial.println(" cm <<<");

        Serial.println("================================");

        mostrarDistanciaLCD(distanciaCm);
    }
    else {
        Serial.println("DISTANCIA NO VALIDA");
        Serial.println("================================");
    }
}

// =====================================================
// ESTADO
// =====================================================

void mostrarEstado() {
    Serial.println();
    Serial.println("----------- ESTADO --------------");

    Serial.print("Ruido calibrado: ");
    Serial.println(ruidoCalibrado ? "SI" : "NO");

    if (ruidoCalibrado) {
        Serial.print("RMS ruido: ");
        Serial.println(ruidoRmsPromedio, 2);

        Serial.print("Pico ruido: ");
        Serial.println(ruidoPicoMaximo, 2);
    }

    Serial.print("Baseline calibrado: ");
    Serial.println(baselineCalibrado ? "SI" : "NO");

    if (baselineCalibrado) {
        Serial.print("RMS baseline: ");
        Serial.println(baselineRms, 2);

        Serial.print("Pico baseline: ");
        Serial.println(baselinePico, 2);
    }

    Serial.print("Offset correlacion: ");
    Serial.println(OFFSET_CORRELACION);

    Serial.println("--------------------------------");
}

// =====================================================
// BOTONES
// =====================================================

void revisarBotones() {
    bool estadoRuido = digitalRead(PIN_BOTON_RUIDO);
    bool estadoBaseline = digitalRead(PIN_BOTON_BASELINE);
    bool estadoMedir = digitalRead(PIN_BOTON_MEDIR);

    unsigned long ahora = millis();

    if (estadoRuido != estadoAnteriorRuido) {
        if (ahora - ultimoCambioRuido >= DEBOUNCE_MS) {
            ultimoCambioRuido = ahora;

            if (estadoRuido == LOW) {
                Serial.println();
                Serial.println("Boton R presionado");
                calibrarRuido();
            }
        }

        estadoAnteriorRuido = estadoRuido;
    }

    if (estadoBaseline != estadoAnteriorBaseline) {
        if (ahora - ultimoCambioBaseline >= DEBOUNCE_MS) {
            ultimoCambioBaseline = ahora;

            if (estadoBaseline == LOW) {
                Serial.println();
                Serial.println("Boton B presionado");
                calibrarBaseline();
            }
        }

        estadoAnteriorBaseline = estadoBaseline;
    }

    if (estadoMedir != estadoAnteriorMedir) {
        if (ahora - ultimoCambioMedir >= DEBOUNCE_MS) {
            ultimoCambioMedir = ahora;

            if (estadoMedir == LOW) {
                Serial.println();
                Serial.println("Boton X presionado");
                medir();
            }
        }

        estadoAnteriorMedir = estadoMedir;
    }
}

// =====================================================
// SETUP
// =====================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(PIN_BOTON_RUIDO, INPUT_PULLUP);
    pinMode(PIN_BOTON_BASELINE, INPUT_PULLUP);
    pinMode(PIN_BOTON_MEDIR, INPUT_PULLUP);

    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();

    generarChirp();
    generarFiltroFIR();

    dacWrite(PIN_DAC, DAC_CENTRO);
    configurarI2S();

    for (int i = 0; i < N; i++) {
        baseline[i] = 0.0f;
        residual[i] = 0.0f;
    }

    Serial.println();
    Serial.println("================================");
    Serial.println(" RADAR ACUSTICO LISTO");
    Serial.println("================================");
    Serial.println("Fs = 86 kHz");
    Serial.println("Chirp = 4-10 kHz");
    Serial.println("Duracion = 2 ms");
    Serial.println("A = 10");
    Serial.println("FIR = 3.5-11 kHz");
    Serial.println("Offset correlacion = 221");

    Serial.println();
    Serial.println("Comandos Serial:");
    Serial.println("r = calibrar ruido");
    Serial.println("b = calibrar baseline");
    Serial.println("x = medir distancia");
    Serial.println("s = mostrar estado");

    Serial.println();
    Serial.println("Botones:");
    Serial.println("GPIO27 = ruido");
    Serial.println("GPIO32 = baseline");
    Serial.println("GPIO33 = medir");

    Serial.println();
    Serial.println("Secuencia:");
    Serial.println("1. R");
    Serial.println("2. B sin objeto");
    Serial.println("3. colocar objeto");
    Serial.println("4. X");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
    revisarBotones();

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
    else if (comando == 'b') {
        calibrarBaseline();
    }
    else if (comando == 'x') {
        medir();
    }
    else if (comando == 's') {
        mostrarEstado();
    }
}