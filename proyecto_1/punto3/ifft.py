import numpy as np

from fft import fft


# Entrada: X -- lista de N complejos en frecuencia
# Salida: x -- lista de N complejos en el tiempo
# Restriccion:    debe ser potencia de 2
def ifft(X):
    N = len(X)
    # la inversa se obtiene conjugando, aplicando la FFT directa y
    # conjugando de nuevo; al final se divide entre N
    return np.conj(fft(np.conj(X))) / N