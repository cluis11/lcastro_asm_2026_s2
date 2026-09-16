import numpy as np
import time

from fft import fft
from ifft import ifft


# Entrada: x -- senal recibida (larga)
#          h -- senal conocida que se busca (corta)
# Salida: r -- correlacion, equivalente a la version directa
# Restriccion: len(h) no debe superar len(x)
def correlacion_fft(x, h):
    N = len(x)
    M = len(h)

    L = N + M - 1 # largo minimo para evitar contaminacion
    L = 2 ** int(np.ceil(np.log2(L))) # redondear a potencia de 2 para la FFT

    x_pad = np.zeros(L)
    h_pad = np.zeros(L)
    x_pad[:N] = x # rellenar con ceros hasta L
    h_pad[:M] = h

    X = fft(x_pad)
    H = fft(h_pad)
    R = np.conj(H) * X # np.conj: conjugado, convierte en correlacion

    r = np.real(ifft(R)) # np.real: descarta residuo imaginario
    return r[:N - M + 1] # recortar al largo de la version directa

if __name__ == "__main__":
    from senal import generar_chirp, generar_recibida, calcular_distancia
    from correlacion import correlacion, retardo_desde_fuga

    chirp = generar_chirp()
    retardo_real = 1200
    recibida = generar_recibida(chirp, [retardo_real], [0.3])

    r_directa = correlacion(recibida, chirp)
    r_fft = correlacion_fft(recibida, chirp)

    print("=== Correlacion directa vs FFT ===")
    print(f"Retardo real:    {retardo_real}")
    print(f"Directa detecta: {retardo_desde_fuga(r_directa, len(chirp))}")
    print(f"FFT detecta:     {retardo_desde_fuga(r_fft, len(chirp))}")
    print(f"Coinciden:       {np.allclose(r_directa, r_fft)}")

    print("\n=== Comparacion de tiempos ===")
    print(f"{'N':>6} {'Directa (s)':>13} {'FFT (s)':>10}")

    for Ni in [512, 1024, 2048, 4096]:
        x = np.random.rand(Ni)
        h = chirp

        t0 = time.perf_counter()
        correlacion(x, h)
        t_dir = time.perf_counter() - t0

        t0 = time.perf_counter()
        correlacion_fft(x, h)
        t_fft = time.perf_counter() - t0

        print(f"{Ni:>6} {t_dir:>13.4f} {t_fft:>10.4f}")