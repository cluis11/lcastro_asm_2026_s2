import numpy as np


# Entrada: X -- lista de N complejos en frecuencia
# Salida: x -- lista de N complejos en el tiempo
# Restriccion: ninguna sobre N
def idft(X):
    N = len(X)
    x = np.zeros(N, dtype=complex)
    for n in range(N):
        for k in range(N):
            # exponente positivo (sin el signo menos de la directa)
            x[n] += X[k] * np.exp(2j * np.pi * k * n / N)
    return x / N # division entre N