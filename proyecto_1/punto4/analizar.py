import time

import numpy as np
import matplotlib.pyplot as plt
import serial

PUERTO = '/dev/ttyUSB0'
BAUD = 115200

# deben coincidir con los del programa del ESP32
N = 2048
M = 200
F0 = 0.10
F1 = 0.40

V = 343.0          # velocidad del sonido, m/s


# Entrada:      ninguna
# Salida:       fs medida en Hz y arreglo con las N muestras capturadas
# Restriccion:  el ESP32 debe estar corriendo el programa de captura
def capturar():
    with serial.Serial(PUERTO, BAUD, timeout=10) as s:
        time.sleep(2.5)                 # esperar el reinicio del ESP32
        s.reset_input_buffer()
        s.write(b'x')                   # disparar una medicion

        # descartar todo hasta encontrar el inicio de un bloque limpio
        while True:
            linea = s.readline().decode(errors='ignore').strip()
            if linea == '--- INICIO ---':
                break
            if not linea:
                raise RuntimeError("el ESP32 no respondio")

        fs = None
        muestras = []

        while True:
            linea = s.readline().decode(errors='ignore').strip()
            if linea == '--- FIN ---':
                break
            if not linea:
                raise RuntimeError("se corto la transmision")
            if linea.startswith('fs_medida_Hz:'):
                try:
                    fs = float(linea.split(':')[1])
                except ValueError:
                    pass
            elif linea.isdigit():
                muestras.append(int(linea))

    if fs is None:
        raise RuntimeError("no se pudo leer la frecuencia de muestreo")
    if len(muestras) < N // 2:
        raise RuntimeError(f"solo llegaron {len(muestras)} muestras de {N}")

    return fs, np.array(muestras, dtype=float)


# Entrada:      ninguna, usa los parametros globales
# Salida:       arreglo con las M muestras del chirp, igual al del ESP32
# Restriccion:  F0 y F1 en frecuencia normalizada
def generar_chirp():
    n = np.arange(M)
    fase = 2 * np.pi * (F0 * n + (F1 - F0) / (2.0 * M) * n * n)
    return np.sin(fase)


# Entrada:      x -- senal capturada, ventana -- largo de la media movil
# Salida:       senal sin la deriva lenta del nivel de continua
# Restriccion:  la ventana debe ser mayor que el periodo de la senal util
def quitar_deriva(x, ventana=101):
    kernel = np.ones(ventana) / ventana
    base = np.convolve(x, kernel, mode='same')   # tendencia lenta
    return x - base                              # filtro pasa-altos


if __name__ == "__main__":
    fs, datos = capturar()
    print(f"fs medida:  {fs:.1f} Hz")
    print(f"muestras:   {len(datos)}")

    datos = quitar_deriva(datos)

    chirp = generar_chirp()
    r = np.correlate(datos, chirp, mode='valid')
    env = np.abs(r)

    fuga = int(np.argmax(env))
    inicio = fuga + M
    eco = None

    if inicio < len(env):
        eco = inicio + int(np.argmax(env[inicio:]))
        retardo = eco - fuga
        d = V * (retardo / fs) / 2
        print(f"pico fuga:  muestra {fuga}  (valor {env[fuga]:.0f})")
        print(f"pico eco:   muestra {eco}  (valor {env[eco]:.0f})")
        print(f"ruido medio:{np.median(env[inicio:]):.0f}")
        print(f"retardo:    {retardo} muestras")
        print(f"distancia:  {d:.3f} m")
    else:
        print("no queda senal despues de la zona ciega")

    fig, axes = plt.subplots(2, 1, figsize=(11, 7))

    axes[0].plot(datos, linewidth=0.6)
    axes[0].set_title('Senal capturada, sin deriva')
    axes[0].set_xlabel('Muestras')
    axes[0].grid(True)

    axes[1].plot(env, linewidth=0.7)
    axes[1].axvline(fuga, color='green', linestyle='--', alpha=0.6, label=f'Fuga ({fuga})')
    if eco is not None:
        axes[1].axvline(eco, color='red', linestyle='--', alpha=0.6, label=f'Eco ({eco})')
    axes[1].set_title('Correlacion con el chirp transmitido')
    axes[1].set_xlabel('Retardo (muestras)')
    axes[1].legend()
    axes[1].grid(True)

    plt.tight_layout()
    plt.savefig('captura_real.png', dpi=150)
    print("Grafica guardada en captura_real.png")