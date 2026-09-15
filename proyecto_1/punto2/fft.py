import numpy as np


# Entrada:      x -- lista de N muestras de una senal
# Salida:       X -- lista de N complejos, igual al resultado de dft(x)
# Restriccion:  N debe ser potencia de 2
def fft(x):
    x = np.asarray(x, dtype=complex)    # convierte la lista a arreglo complejo
    N = len(x)

    if N == 1:
        return x

    pares = fft(x[0::2])                # x[0::2]: indices pares
    impares = fft(x[1::2])              # x[1::2]: indices impares

    X = np.zeros(N, dtype=complex)      # arreglo de N ceros complejos
    for k in range(N // 2):
        # factor de giro (twiddle) por formula de Euler: e^(-j2*pi*k/N)
        giro = np.exp(-2j * np.pi * k / N) * impares[k]
        # mariposa: el mismo giro produce dos salidas, sumando y restando
        X[k] = pares[k] + giro
        X[k + N // 2] = pares[k] - giro
    return X