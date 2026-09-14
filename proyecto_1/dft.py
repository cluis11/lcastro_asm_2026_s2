import numpy as np


# Entrada:      x -- lista de N muestras de una senal
# Salida:       X -- lista de N complejos, uno por frecuencia
# Restriccion:  ninguna sobre N
def dft(x):
    N = len(x)
    X = np.zeros(N, dtype=complex)      # arreglo de N ceros complejos
    for k in range(N):
        for n in range(N):
            # formula de Euler: e^(-j2*pi*k*n/N) = cos(...) - j*sen(...)
            X[k] += x[n] * np.exp(-2j * np.pi * k * n / N)
    return X