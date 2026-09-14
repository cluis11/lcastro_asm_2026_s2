import numpy as np

def dft(x):
    N = len(x)
    X = np.zeros(N, dtype=complex)
    for k in range(N):
        for n in range(N):
            X[k] += x[n] * np.exp(-2j * np.pi * k * n / N)
    return X

N = 8
n = np.arange(N)

# Ejemplo 1: una sola frecuencia
x1 = np.cos(2 * np.pi * 1 * n / N)

print("=== Ejemplo 1: una frecuencia (k=1) ===")
print("Muestras:", np.round(x1, 3))
print("Magnitud:", np.round(np.abs(dft(x1)), 3))

# Ejemplo 2: dos frecuencias mezcladas
x2 = np.cos(2 * np.pi * 1 * n / N) + 2 * np.cos(2 * np.pi * 3 * n / N)

print("\n=== Ejemplo 2: dos frecuencias (k=1 y k=3) ===")
print("Muestras:", np.round(x2, 3))
print("Magnitud:", np.round(np.abs(dft(x2)), 3))