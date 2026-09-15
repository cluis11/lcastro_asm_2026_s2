import time
import numpy as np
import matplotlib.pyplot as plt

from dft import dft
from fft import fft


# Entregable 2a: verificacion de las implementaciones
def verificar():
    N = 8
    n = np.arange(N)                    # np.arange: indices [0, 1, ..., N-1]

    # Una sola frecuencia
    x1 = np.cos(2 * np.pi * 1 * n / N)  # np.cos: coseno en radianes
    print("=== Ejemplo 1: una frecuencia (k=1) ===")
    print("Muestras:", np.round(x1, 3))         # np.round: redondea a 3 decimales
    print("Magnitud:", np.round(np.abs(dft(x1)), 3))    # np.abs: magnitud del complejo

    # Dos frecuencias mezcladas, la de k=3 con el doble de amplitud
    x2 = np.cos(2 * np.pi * 1 * n / N) + 2 * np.cos(2 * np.pi * 3 * n / N)
    print("\n=== Ejemplo 2: dos frecuencias (k=1 y k=3) ===")
    print("Muestras:", np.round(x2, 3))
    print("Magnitud:", np.round(np.abs(dft(x2)), 3))

    # La FFT debe reproducir exactamente el resultado de la DFT
    print("\n=== Comparacion DFT vs FFT ===")
    print("DFT:", np.round(np.abs(dft(x2)), 3))
    print("FFT:", np.round(np.abs(fft(x2)), 3))
    print("Coinciden:", np.allclose(dft(x2), fft(x2)))  # np.allclose: compara con tolerancia


# Entregable 2b: comparacion de tiempos de ejecucion
def comparar_tiempos():
    print("\n=== Comparacion de tiempos ===")
    print(f"{'N':>6} {'DFT (s)':>10} {'FFT (s)':>10}")

    Ns = [64, 128, 256, 512, 1024]
    t_dfts = []
    t_ffts = []

    for Ni in Ns:
        x = np.random.rand(Ni)          # np.random.rand: senal aleatoria de Ni muestras

        t0 = time.perf_counter()        # marca de tiempo inicial
        dft(x)
        t_dfts.append(time.perf_counter() - t0)

        t0 = time.perf_counter()
        fft(x)
        t_ffts.append(time.perf_counter() - t0)

        print(f"{Ni:>6} {t_dfts[-1]:>10.4f} {t_ffts[-1]:>10.4f}")

    plt.figure()                        # figura nueva para no encimar graficas
    plt.plot(Ns, t_dfts, 'o-', label='DFT')
    plt.plot(Ns, t_ffts, 's-', label='FFT')
    plt.xlabel('N')
    plt.ylabel('Tiempo (s)')
    plt.yscale('log')                   # escala log: sin esto la FFT se pega al eje
    plt.legend()
    plt.grid(True)
    plt.savefig('tiempos.png', dpi=150)


# Entregable 2c: magnitud y fase de diferentes senales
def graficar_magnitud_fase():
    N = 64
    n = np.arange(N)

    senales = {
        'Coseno': np.cos(2 * np.pi * 5 * n / N),
        'Seno': np.sin(2 * np.pi * 5 * n / N),
        'Suma de dos': np.cos(2*np.pi*3*n/N) + 2*np.sin(2*np.pi*10*n/N),
    }

    fig, axes = plt.subplots(len(senales), 3, figsize=(14, 9))

    for i, (nombre, x) in enumerate(senales.items()):
        X = fft(x)
        mag = np.abs(X)
        fase = np.angle(X)              # np.angle: fase en radianes
        fase[mag < 1e-6] = 0            # descarta fase donde no hay senal

        axes[i][0].plot(n, x)
        axes[i][0].set_title(f'{nombre} - tiempo')
        axes[i][1].stem(mag)
        axes[i][1].set_title('Magnitud')
        axes[i][2].stem(fase)
        axes[i][2].set_title('Fase')
        axes[i][2].set_ylim(-3.5, 3.5)  # eje fijo: permite comparar entre filas

    plt.tight_layout()
    plt.savefig('magnitud_fase.png', dpi=150)


if __name__ == "__main__":
    verificar()
    comparar_tiempos()
    graficar_magnitud_fase()