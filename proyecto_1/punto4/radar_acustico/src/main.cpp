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
// BOTON DE MEDICION
// =====================================================

const int PIN_BOTON_MEDIR = 33;
const unsigned long DEBOUNCE_MS = 50;

bool estadoAnteriorMedir = HIGH;
unsigned long ultimoCambioMedir = 0;

// =====================================================
// GEOMETRIA Y DISTANCIA
// =====================================================

const float VELOCIDAD_SONIDO = 343.0f;
const float SEPARACION_TX_RX = 0.10f;
const int OFFSET_CORRELACION = 221;

// Rango de busqueda actual. Se validara experimentalmente.
const float DISTANCIA_MIN_METROS = 0.50f;
const float DISTANCIA_MAX_METROS = 1.50f;

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
// FFT Y CORRELACION
// =====================================================

// Overlap-save:
// FFT_N = 256
// solape = M - 1 = 171
// muestras nuevas por bloque = 256 - 171 = 85
// La correlacion se obtiene como convolucion con el chirp invertido.

const int FFT_N = 256;
const int FFT_SOLAPE = M - 1;
const int FFT_BLOQUE_NUEVO = FFT_N - FFT_SOLAPE;

float fftReal[FFT_N];
float fftImag[FFT_N];
float referenciaFFTReal[FFT_N];
float referenciaFFTImag[FFT_N];

// =====================================================
// BUFFERS DSP
// =====================================================

uint16_t datos[N];
float centrado[N];
float filtrado[N];

// =====================================================
// RESULTADOS
// =====================================================

struct ResultadoCorrelacion {
    bool valida;
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
// FFT RADIX-2
// =====================================================

void fftRadix2(float *real, float *imag, int n, bool inversa) {
    int j = 0;

    for (int i = 1; i < n; i++) {
        int bit = n >> 1;

        while (j & bit) {
            j ^= bit;
            bit >>= 1;
        }

        j ^= bit;

        if (i < j) {
            float tempReal = real[i];
            real[i] = real[j];
            real[j] = tempReal;

            float tempImag = imag[i];
            imag[i] = imag[j];
            imag[j] = tempImag;
        }
    }

    for (int longitud = 2; longitud <= n; longitud <<= 1) {
        float angulo = 2.0f * PI / longitud;
        if (!inversa) {
            angulo = -angulo;
        }

        float wLenReal = cosf(angulo);
        float wLenImag = sinf(angulo);
        int mitad = longitud >> 1;

        for (int inicio = 0; inicio < n; inicio += longitud) {
            float wReal = 1.0f;
            float wImag = 0.0f;

            for (int k = 0; k < mitad; k++) {
                int par = inicio + k;
                int impar = par + mitad;

                float tReal = real[impar] * wReal - imag[impar] * wImag;
                float tImag = real[impar] * wImag + imag[impar] * wReal;
                float uReal = real[par];
                float uImag = imag[par];

                real[par] = uReal + tReal;
                imag[par] = uImag + tImag;
                real[impar] = uReal - tReal;
                imag[impar] = uImag - tImag;

                float nuevoWReal = wReal * wLenReal - wImag * wLenImag;
                float nuevoWImag = wReal * wLenImag + wImag * wLenReal;

                wReal = nuevoWReal;
                wImag = nuevoWImag;
            }
        }
    }

    if (inversa) {
        float escala = 1.0f / n;
        for (int i = 0; i < n; i++) {
            real[i] *= escala;
            imag[i] *= escala;
        }
    }
}

// =====================================================
// GENERACION DE FIR
// =====================================================

float sincF(float x) {
    if (fabsf(x) < 1e-8f) {
        return 1.0f;
    }
    return sinf(PI * x) / (PI * x);
}

void generarFiltroFIR() {
    const int centro = (FIR_TAPS - 1) / 2;

    for (int i = 0; i < FIR_TAPS; i++) {
        float n = (float)(i - centro);
        float hMax = 2.0f * FIR_F_MAX / FS * sincF(2.0f * FIR_F_MAX * n / FS);
        float hMin = 2.0f * FIR_F_MIN / FS * sincF(2.0f * FIR_F_MIN * n / FS);
        float h = hMax - hMin;
        float ventana = 0.54f - 0.46f * cosf(2.0f * PI * i / (FIR_TAPS - 1));

        fir[i] = h * ventana;
    }
}

// =====================================================
// GENERACION DEL CHIRP Y REFERENCIA FFT
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

void prepararReferenciaFFT() {
    for (int i = 0; i < FFT_N; i++) {
        referenciaFFTReal[i] = 0.0f;
        referenciaFFTImag[i] = 0.0f;
    }

    // Matched filter: chirp invertido en el tiempo.
    for (int i = 0; i < M; i++) {
        referenciaFFTReal[i] = chirpReferencia[M - 1 - i];
    }

    fftRadix2(referenciaFFTReal, referenciaFFTImag, FFT_N, false);
}

// =====================================================
// ADC / I2S
// =====================================================

void transmitirChirp();

void configurarI2S() {
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

void vaciarDMA() {
    uint16_t buffer[256];
    size_t bytesLeidos = 0;

    for (int k = 0; k < 4; k++) {
        i2s_read(I2S_PORT, buffer, sizeof(buffer), &bytesLeidos, portMAX_DELAY);
    }
}

float capturarADCActivo(bool emitirChirp) {
    size_t bytesLeidos = 0;
    vaciarDMA();

    uint32_t tInicio = micros();

    if (emitirChirp) {
        transmitirChirp();
    }

    esp_err_t err = i2s_read(I2S_PORT, datos, sizeof(datos), &bytesLeidos, portMAX_DELAY);
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
// TRANSMISION
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
// PREPROCESAMIENTO
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

float calcularRMS(const float *senal, int inicio = 0) {
    if (inicio < 0) {
        inicio = 0;
    }
    if (inicio >= N) {
        return 0.0f;
    }

    double suma = 0.0;
    for (int i = inicio; i < N; i++) {
        suma += (double)senal[i] * senal[i];
    }

    return sqrt(suma / (N - inicio));
}

// =====================================================
// RANGO DE BUSQUEDA
// =====================================================

int indiceDesdeDistancia(float distanciaMetros) {
    float mitadSeparacion = SEPARACION_TX_RX * 0.5f;
    float mitadRecorrido = sqrtf(distanciaMetros * distanciaMetros + mitadSeparacion * mitadSeparacion);
    float recorrido = 2.0f * mitadRecorrido;
    float tiempoVuelo = recorrido / VELOCIDAD_SONIDO;
    int muestrasVuelo = (int)roundf(tiempoVuelo * FS);

    return OFFSET_CORRELACION + muestrasVuelo;
}

// =====================================================
// CORRELACION MEDIANTE FFT + OVERLAP-SAVE
// =====================================================

ResultadoCorrelacion correlacionarFFT() {
    ResultadoCorrelacion resultado = {false, -1, 0.0f, 0.0f};

    int indiceMinimo = indiceDesdeDistancia(DISTANCIA_MIN_METROS);
    int indiceMaximo = indiceDesdeDistancia(DISTANCIA_MAX_METROS);
    int ultimoDesplazamiento = N - M;

    if (indiceMinimo < FIR_DESCARTE_INICIAL) {
        indiceMinimo = FIR_DESCARTE_INICIAL;
    }
    if (indiceMaximo > ultimoDesplazamiento) {
        indiceMaximo = ultimoDesplazamiento;
    }

    float maxAbsoluto = 0.0f;
    const int ultimoIndiceConvolucion = N + M - 2;

    for (int primerIndiceSalida = 0;
         primerIndiceSalida <= ultimoIndiceConvolucion;
         primerIndiceSalida += FFT_BLOQUE_NUEVO) {

        int primerIndiceEntrada = primerIndiceSalida - FFT_SOLAPE;

        for (int i = 0; i < FFT_N; i++) {
            int indiceGlobal = primerIndiceEntrada + i;
            fftReal[i] = (indiceGlobal >= 0 && indiceGlobal < N) ? filtrado[indiceGlobal] : 0.0f;
            fftImag[i] = 0.0f;
        }

        // FFT del bloque recibido.
        fftRadix2(fftReal, fftImag, FFT_N, false);

        // Producto espectral con la referencia del matched filter.
        for (int k = 0; k < FFT_N; k++) {
            float xr = fftReal[k];
            float xi = fftImag[k];
            float hr = referenciaFFTReal[k];
            float hi = referenciaFFTImag[k];

            fftReal[k] = xr * hr - xi * hi;
            fftImag[k] = xr * hi + xi * hr;
        }

        // Regreso al dominio temporal.
        fftRadix2(fftReal, fftImag, FFT_N, true);

        // Se descartan las primeras M-1 muestras contaminadas por solape.
        for (int i = FFT_SOLAPE; i < FFT_N; i++) {
            int indiceConvolucion = primerIndiceSalida + (i - FFT_SOLAPE);
            if (indiceConvolucion > ultimoIndiceConvolucion) {
                break;
            }

            // y[d + M - 1] corresponde a la correlacion para desplazamiento d.
            int desplazamiento = indiceConvolucion - FFT_SOLAPE;

            if (desplazamiento < indiceMinimo || desplazamiento > indiceMaximo) {
                continue;
            }

            float valor = fftReal[i];
            float absoluto = fabsf(valor);

            if (absoluto > maxAbsoluto) {
                maxAbsoluto = absoluto;
                resultado.indice = desplazamiento;
                resultado.valor = valor;
                resultado.valida = true;
            }
        }
    }

    if (resultado.valida) {
        resultado.tiempoMs = 1000.0f * resultado.indice / FS;
    }

    return resultado;
}

// =====================================================
// CALCULO DE DISTANCIA
// =====================================================

ResultadoDistancia calcularDistancia(int indiceCorrelacion) {
    ResultadoDistancia resultado = {};
    resultado.indiceCorregido = indiceCorrelacion - OFFSET_CORRELACION;

    if (resultado.indiceCorregido <= 0) {
        return resultado;
    }

    float tiempoVuelo = (float)resultado.indiceCorregido / FS;
    float recorrido = tiempoVuelo * VELOCIDAD_SONIDO;
    float mitadRecorrido = recorrido * 0.5f;
    float mitadSeparacion = SEPARACION_TX_RX * 0.5f;
    float argumento = mitadRecorrido * mitadRecorrido - mitadSeparacion * mitadSeparacion;

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
// MEDICION COMPLETA
// =====================================================

void medir() {
    float tiempoCaptura = capturarADC(true);
    float media = quitarDC();

    uint32_t tFiltroInicio = micros();
    aplicarFiltroFIR();
    uint32_t tFiltroFin = micros();

    uint32_t tFFTInicio = micros();
    ResultadoCorrelacion correlacion = correlacionarFFT();
    uint32_t tFFTFin = micros();

    float rmsFiltrada = calcularRMS(filtrado, FIR_DESCARTE_INICIAL);
    float tiempoFiltro = (float)(tFiltroFin - tFiltroInicio) / 1000.0f;
    float tiempoFFT = (float)(tFFTFin - tFFTInicio) / 1000.0f;
    int indiceMinimo = indiceDesdeDistancia(DISTANCIA_MIN_METROS);
    int indiceMaximo = indiceDesdeDistancia(DISTANCIA_MAX_METROS);

    Serial.println();
    Serial.println("================================");
    Serial.println(" RADAR FFT");
    Serial.println("================================");
    Serial.print("Tiempo captura: ");
    Serial.print(tiempoCaptura, 3);
    Serial.println(" ms");
    Serial.print("Media ADC: ");
    Serial.println(media, 2);
    Serial.print("Tiempo FIR: ");
    Serial.print(tiempoFiltro, 3);
    Serial.println(" ms");
    Serial.print("RMS filtrada: ");
    Serial.println(rmsFiltrada, 2);
    Serial.print("Rango declarado: ");
    Serial.print(DISTANCIA_MIN_METROS * 100.0f, 0);
    Serial.print(" - ");
    Serial.print(DISTANCIA_MAX_METROS * 100.0f, 0);
    Serial.println(" cm");
    Serial.print("Busqueda indices: ");
    Serial.print(indiceMinimo);
    Serial.print(" - ");
    Serial.println(indiceMaximo);

    Serial.println();
    Serial.println("--------- FFT + CORRELACION --------");
    Serial.print("FFT N: ");
    Serial.println(FFT_N);
    Serial.print("Tiempo FFT/correlacion: ");
    Serial.print(tiempoFFT, 3);
    Serial.println(" ms");

    if (!correlacion.valida) {
        Serial.println("No se encontro eco valido.");
        return;
    }

    Serial.print("Pico correlacion FFT: ");
    Serial.println(correlacion.valor, 2);
    Serial.print("Indice correlacion FFT: ");
    Serial.println(correlacion.indice);
    Serial.print("Tiempo indice FFT: ");
    Serial.print(correlacion.tiempoMs, 3);
    Serial.println(" ms");

    ResultadoDistancia distancia = calcularDistancia(correlacion.indice);

    Serial.println();
    Serial.println("================================");
    Serial.println(" RESULTADO DISTANCIA");
    Serial.println("================================");

    if (!distancia.valida) {
        Serial.println("DISTANCIA NO VALIDA");
        Serial.println("================================");
        return;
    }

    float distanciaCm = distancia.distanciaMetros * 100.0f;

    Serial.print("Indice FFT: ");
    Serial.println(correlacion.indice);
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

// =====================================================
// ESTADO Y ENTRADAS
// =====================================================

void mostrarEstado() {
    Serial.println();
    Serial.println("----------- ESTADO --------------");
    Serial.println("Sistema: correlacion por FFT");
    Serial.print("FFT N: ");
    Serial.println(FFT_N);
    Serial.print("Rango: ");
    Serial.print(DISTANCIA_MIN_METROS * 100.0f, 0);
    Serial.print(" - ");
    Serial.print(DISTANCIA_MAX_METROS * 100.0f, 0);
    Serial.println(" cm");
    Serial.print("Indice minimo: ");
    Serial.println(indiceDesdeDistancia(DISTANCIA_MIN_METROS));
    Serial.print("Indice maximo: ");
    Serial.println(indiceDesdeDistancia(DISTANCIA_MAX_METROS));
    Serial.println("--------------------------------");
}

void revisarBoton() {
    bool estadoMedir = digitalRead(PIN_BOTON_MEDIR);
    unsigned long ahora = millis();

    if (estadoMedir != estadoAnteriorMedir) {
        if (ahora - ultimoCambioMedir >= DEBOUNCE_MS) {
            ultimoCambioMedir = ahora;

            if (estadoMedir == LOW) {
                Serial.println();
                Serial.println("Boton medir presionado");
                medir();
            }
        }

        estadoAnteriorMedir = estadoMedir;
    }
}

// =====================================================
// SETUP Y LOOP
// =====================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    pinMode(PIN_BOTON_MEDIR, INPUT_PULLUP);

    Wire.begin(LCD_SDA, LCD_SCL);
    lcd.init();
    lcd.backlight();

    generarChirp();
    generarFiltroFIR();
    prepararReferenciaFFT();

    dacWrite(PIN_DAC, DAC_CENTRO);
    configurarI2S();

    Serial.println();
    Serial.println("================================");
    Serial.println(" RADAR ACUSTICO FFT");
    Serial.println("================================");
    Serial.println("Fs = 86 kHz");
    Serial.println("Chirp = 4-10 kHz");
    Serial.println("Duracion = 2 ms");
    Serial.println("FIR = 3.5-11 kHz");
    Serial.println("Procesamiento: ADC -> DC -> FIR -> FFT -> IFFT -> correlacion -> ToF -> distancia");
    Serial.print("Rango = ");
    Serial.print(DISTANCIA_MIN_METROS * 100.0f, 0);
    Serial.print(" - ");
    Serial.print(DISTANCIA_MAX_METROS * 100.0f, 0);
    Serial.println(" cm");
    Serial.print("Indices buscados = ");
    Serial.print(indiceDesdeDistancia(DISTANCIA_MIN_METROS));
    Serial.print(" - ");
    Serial.println(indiceDesdeDistancia(DISTANCIA_MAX_METROS));
    Serial.println("La distancia sale solo de correlacion FFT.");
    Serial.println("Comandos: x = medir, s = estado");
}

void loop() {
    revisarBoton();

    if (!Serial.available()) {
        return;
    }

    char comando = Serial.read();
    while (Serial.available()) {
        Serial.read();
    }

    if (comando == 'x') {
        medir();
    } else if (comando == 's') {
        mostrarEstado();
    }
}