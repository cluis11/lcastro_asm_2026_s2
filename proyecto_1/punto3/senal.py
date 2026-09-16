import numpy as np

fs = 44100 # frecuencia de muestreo, en muestras por segundo
dur = 0.05 # duracion del chirp, en segundos
f0 = 2000 # frecuencia inicial del chirp, en Hz
f1 = 8000 # frecuencia final del chirp, en Hz
N = 16384 # cantidad de muestras de la senal recibida
v = 343.0 # velocidad del sonido, en m/s


# Entrada: ninguna, usa los parametros globales
# Salida: arreglo con las muestras del chirp
# Restriccion: f1 debe ser menor que fs/2 (teorema de muestreo)
def generar_chirp():
    t = np.arange(0, dur, 1/fs) # np.arange: instantes de muestreo
    # chirp lineal: la frecuencia sube de f0 a f1 durante dur segundos
    return np.sin(2*np.pi*(f0*t + (f1-f0)/(2*dur)*t**2))


# Entrada: chirp -- senal transmitida
#          retardos -- lista de retardos de cada eco, en muestras
#          atenuaciones -- lista de amplitudes de cada eco
#          ruido -- amplitud del ruido de fondo
#          desfase -- muestras entre el inicio de la captura y la emision
# Salida: arreglo de N muestras con la senal recibida simulada
# Restriccion: retardo + len(chirp) no debe superar N
def generar_recibida(chirp, retardos, atenuaciones, ruido=0.2, desfase=0):
    recibida = np.zeros(N) # arreglo de N ceros
    recibida[desfase:desfase+len(chirp)] += chirp # fuga directa del parlante al microfono

    for retardo, atenuacion in zip(retardos, atenuaciones):
        ini = desfase + retardo
        recibida[ini:ini+len(chirp)] += atenuacion * chirp

    recibida += ruido * np.random.randn(N) # np.random.randn: ruido gaussiano
    return recibida


# Entrada: retardo en muestras
# Salida: distancia al objeto en metros
# Restriccion: configuracion monostatica (transmisor y receptor juntos)
def calcular_distancia(retardo):
    tau = retardo / fs # convierte muestras a segundos
    return v * tau / 2 # formula del enunciado: d = v*tau/2


if __name__ == "__main__":
    chirp = generar_chirp()
    retardo_real = 1200

    recibida = generar_recibida(chirp, [retardo_real], [0.3])

    print(f"Muestras del chirp:     {len(chirp)}")
    print(f"Duracion del chirp:     {dur*1000:.1f} ms")
    print(f"Muestras de la senal:   {N}")
    print(f"Retardo simulado:       {retardo_real} muestras")
    print(f"Tiempo de vuelo:        {retardo_real/fs*1000:.2f} ms")
    print(f"Distancia simulada:     {calcular_distancia(retardo_real):.3f} m")