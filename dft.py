import numpy as np
import time

#Entradas: lista de N muestras de una señal x[n]
#Salidas: lista de N complejos, uno por frecuencia
#Restriccion: ninguna 
def dft(x):
    N = len(x)
    X = np.zeros(N, dtype=complex) #arreglo de N ceros complejos
    for k in range(N):
        for n in range(N):
            # formula de Euler: e^(-j2*pi*k*n/N) = cos(...) - j*sen(...)
            X[k] += x[n] * np.exp(-2j * np.pi * k * n / N)
    return X

#Entradas: lista de N muestras de una señal x[n]
#Salidas: lista de N complejos, uno por frecuencia
#Restriccion: N debe ser potencia de 2
def fft(x):
    x = np.asarray(x, dtype=complex) #convierte la lista a un arreglo complejo
    N = len(x)

    if N == 1:
        return x

    pares = fft(x[0::2]) #indices pares
    impares = fft(x[1::2]) #indices impares

    X = np.zeros(N, dtype=complex) #arreglo de N ceros complejos
    for k in range(N // 2):
        # factor de giro (twiddle) por formula de Euler: e^(-j2*pi*k/N)
        giro = np.exp(-2j * np.pi * k / N) * impares[k]
        # mariposa: el mismo giro produce dos salidas, sumando y restando
        X[k] = pares[k] + giro
        X[k + N // 2] = pares[k] - giro

    return X


N = 8
n = np.arange(N)
 
# Ejemplo 1: una sola frecuencia
x1 = np.cos(2 * np.pi * 1 * n / N) # np.cos: coseno en radianes
 
print("=== Ejemplo 1: una frecuencia (k=1) ===")
print("Muestras:", np.round(x1, 3)) # np.round: redondea a 3 decimales
print("Magnitud:", np.round(np.abs(dft(x1)), 3)) # np.abs: magnitud del complejo
 
# Ejemplo 2: dos frecuencias mezcladas, la de k=3 con el doble de amplitud
x2 = np.cos(2 * np.pi * 1 * n / N) + 2 * np.cos(2 * np.pi * 3 * n / N)
 
print("\n=== Ejemplo 2: dos frecuencias (k=1 y k=3) ===")
print("Muestras:", np.round(x2, 3))
print("Magnitud:", np.round(np.abs(dft(x2)), 3))
 
# Ejemplo 3: la FFT debe reproducir exactamente el resultado de la DFT
print("\n=== Comparacion DFT vs FFT ===")
print("DFT:", np.round(np.abs(dft(x2)), 3))
print("FFT:", np.round(np.abs(fft(x2)), 3))
print("Coinciden:", np.allclose(dft(x2), fft(x2))) # np.allclose: compara con tolerancia


print("\n=== Comparacion de tiempos ===")
print(f"{'N':>6} {'DFT (s)':>10} {'FFT (s)':>10}")

for N in [64, 128, 256, 512, 1024]:
    x = np.random.rand(N)               # senal aleatoria de N muestras

    t0 = time.perf_counter()            # marca de tiempo inicial
    dft(x)
    t_dft = time.perf_counter() - t0

    t0 = time.perf_counter()
    fft(x)
    t_fft = time.perf_counter() - t0

    print(f"{N:>6} {t_dft:>10.4f} {t_fft:>10.4f}")
