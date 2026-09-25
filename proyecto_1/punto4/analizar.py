import time

import numpy as np
import matplotlib.pyplot as plt
import serial

PUERTO = '/dev/ttyUSB0'
BAUD = 115200

N = 2048
M = 150
F0 = 0.10
F1 = 0.40
V = 343.0

MARGEN_EXTRA = 30      # muestras extra a descartar despues de la zona ciega,
                       # para evitar la cola de la fuga
RAZON_MINIMA = 6       # el pico debe ser al menos esto veces el ruido


def capturar():
    with serial.Serial(PUERTO, BAUD, timeout=10) as s:
        time.sleep(2.5)
        s.reset_input_buffer()
        s.write(b'x\n')

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

    return fs, np.array(muestras, dtype=float)


def generar_chirp():
    n = np.arange(M)
    fase = 2 * np.pi * (F0 * n + (F1 - F0) / (2.0 * M) * n * n)
    return np.sin(fase)


if __name__ == "__main__":
    fs, datos = capturar()
    print(f"fs medida:  {fs:.1f} Hz")
    print(f"muestras:   {len(datos)}")

    datos = datos - np.mean(datos)
    chirp = generar_chirp()
    r = np.correlate(datos, chirp, mode='valid')
    env = np.abs(r)

    fuga = int(np.argmax(env))
    inicio = fuga + M + MARGEN_EXTRA          # zona ciega + margen anti-cola

    zona_ciega_m = V * (M / fs) / 2
    margen_m = V * (MARGEN_EXTRA / fs) / 2
    print(f"zona ciega:      {zona_ciega_m:.3f} m (+ {margen_m:.3f} m de margen)")

    if inicio >= len(env):
        print("RECHAZADO: no queda senal despues de la zona ciega")
    else:
        # ruido de referencia: mediana de la zona posterior, EXCLUYENDO
        # una ventana angosta alrededor del propio pico que se va a evaluar
        ruido = float(np.median(env[inicio:]))
        eco = inicio + int(np.argmax(env[inicio:]))
        pico_val = env[eco]
        razon = pico_val / ruido if ruido > 0 else 0
        retardo = eco - fuga
        d = V * (retardo / fs) / 2

        print(f"pico fuga:   muestra {fuga}  valor {env[fuga]:.0f}")
        print(f"pico eco:    muestra {eco}  valor {pico_val:.0f}")
        print(f"ruido medio: {ruido:.0f}")
        print(f"razon pico/ruido: {razon:.1f}  (minimo aceptado: {RAZON_MINIMA})")
        print(f"retardo:     {retardo} muestras")
        print(f"distancia calculada: {d:.3f} m")

        if razon < RAZON_MINIMA:
            print(f">> RECHAZADO: el pico no se destaca lo suficiente del ruido (no hay deteccion confiable)")
        else:
            print(f">> DETECCION ACEPTADA: distancia = {d:.3f} m")

    plt.figure(figsize=(10, 4))
    plt.plot(env)
    plt.axvline(fuga, color='green', linestyle='--', label='fuga')
    plt.axvline(inicio, color='orange', linestyle=':', label='inicio zona util')
    plt.xlabel('Retardo (muestras)')
    plt.ylabel('Correlacion')
    plt.legend()
    plt.grid(True)
    plt.savefig('captura_real.png', dpi=150)
    print("Grafica guardada en captura_real.png")