import time
import numpy as np
import matplotlib.pyplot as plt
import serial

PUERTO = '/dev/ttyUSB1'
BAUD = 115200
N = 2048
M = 100
FS = 8000.0


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

        muestras = []
        while True:
            linea = s.readline().decode(errors='ignore').strip()
            if linea == '--- FIN ---':
                break
            if linea.isdigit():
                muestras.append(int(linea))

    return np.array(muestras, dtype=float)


if __name__ == "__main__":
    datos = capturar()
    print(f"muestras: {len(datos)}")

    datos_c = datos - np.mean(datos)

    # tiempo en milisegundos, y distancia equivalente en metros para cada muestra
    t_ms = np.arange(len(datos_c)) / FS * 1000
    d_m = 343.0 * (np.arange(len(datos_c)) / FS) / 2

    fig, ax1 = plt.subplots(figsize=(14, 6))
    ax1.plot(t_ms, datos_c, linewidth=0.6)
    ax1.set_xlabel('Tiempo (ms)')
    ax1.set_ylabel('Señal (sin nivel DC)')
    ax1.set_title(f'Señal cruda del microfono (M={M}, fs={FS} Hz)')
    ax1.grid(True)

    # eje secundario con la distancia equivalente
    ax2 = ax1.secondary_xaxis('top', functions=(
        lambda t_ms: t_ms / 1000 * 343.0 / 2,
        lambda d: d / (343.0/2) * 1000
    ))
    ax2.set_xlabel('Distancia equivalente (m)')

    # marcar la zona ciega
    zona_ciega_ms = M / FS * 1000
    ax1.axvspan(0, zona_ciega_ms, color='red', alpha=0.15, label='zona ciega (chirp)')
    ax1.legend()

    plt.tight_layout()
    plt.savefig('senal_cruda.png', dpi=150)
    print("Guardado: senal_cruda.png")