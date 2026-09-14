import numpy as np

def dft(x):
    N = len(x)
    X = np.zeros(N, dtype=complex)
    for k in range(N):
        for n in range(N):
            X[k] += x[n] * np.exp(-2j * np.pi * k * n / N)
    return X


# 8 muestras de una onda que da 1 vuelta completa
N = 8
x = [np.cos(2 * np.pi * n / N) for n in range(N)]

print("Las muestras:")
print(np.round(x, 3))

print("\nLa respuesta de la DFT (magnitud):")
print(np.round(np.abs(dft(x)), 3))