import serial
import time
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt


PUERTO = "/dev/ttyUSB1"
BAUDIOS = 115200

FS = 86000.0
N = 2048

MARCA_CRUDOS = "----------- CRUDOS --------------"
MARCA_FILTRADOS = "----------- FILTRADOS -----------"
MARCA_RESIDUAL = "----------- RESIDUAL ------------"
MARCA_FIN = "------------- FIN ---------------"


def crear_carpeta_salida():
    base = Path("graficas")

    if not base.exists():
        base.mkdir()
        return base, 0

    numero = 1

    while True:
        carpeta = Path(f"graficas_{numero}")

        if not carpeta.exists():
            carpeta.mkdir()
            return carpeta, numero

        numero += 1


def leer_bloque(ser, cantidad):
    datos = []

    while len(datos) < cantidad:
        linea = ser.readline().decode("utf-8", errors="ignore").strip()

        try:
            datos.append(float(linea))
        except ValueError:
            pass

    return datos


def ejecutar_medicion():
    ser = serial.Serial(PUERTO, BAUDIOS, timeout=2)

    time.sleep(0.5)
    ser.reset_input_buffer()

    print("Enviando comando x...")
    ser.write(b"x\n")
    ser.flush()

    metricas = {}

    crudos = []
    filtrados = []
    residual = []

    while True:
        linea = ser.readline().decode("utf-8", errors="ignore").strip()

        if not linea:
            continue

        print(linea)

        if linea.startswith("RMS residual:"):
            metricas["rms_residual"] = float(
                linea.split(":", 1)[1].strip()
            )

        elif linea.startswith("Pico residual:"):
            metricas["pico_residual"] = float(
                linea.split(":", 1)[1].strip()
            )

        elif linea.startswith("Residual / ruido RMS:"):
            metricas["relacion_rms"] = float(
                linea.split(":", 1)[1].strip()
            )

        elif linea.startswith("Residual / ruido pico:"):
            metricas["relacion_pico"] = float(
                linea.split(":", 1)[1].strip()
            )

        elif linea == MARCA_CRUDOS:
            print("Recibiendo muestras crudas...")
            crudos = leer_bloque(ser, N)

        elif linea == MARCA_FILTRADOS:
            print("Recibiendo muestras filtradas...")
            filtrados = leer_bloque(ser, N)

        elif linea == MARCA_RESIDUAL:
            print("Recibiendo muestras residuales...")
            residual = leer_bloque(ser, N)

        elif linea == MARCA_FIN:
            break

    ser.close()

    return metricas, crudos, filtrados, residual


def guardar_csv(crudos, filtrados, residual, carpeta, numero):
    tiempo_ms = [
        i / FS * 1000.0
        for i in range(N)
    ]

    media = sum(crudos) / len(crudos)

    centrados = [
        x - media
        for x in crudos
    ]

    df = pd.DataFrame({
        "muestra": range(N),
        "tiempo_ms": tiempo_ms,
        "adc": crudos,
        "adc_centrado": centrados,
        "adc_filtrado": filtrados,
        "residual": residual,
    })

    ruta_csv = carpeta / f"muestras_{numero}.csv"
    df.to_csv(ruta_csv, index=False)

    return df


def graficar(df, carpeta, numero):
    plt.figure(figsize=(12, 5))
    plt.plot(df["tiempo_ms"], df["adc"])
    plt.xlabel("Tiempo (ms)")
    plt.ylabel("ADC")
    plt.title(f"Señal cruda - medición {numero}")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(
        carpeta / f"senal_cruda_{numero}.png",
        dpi=150
    )
    plt.close()

    plt.figure(figsize=(12, 5))
    plt.plot(df["tiempo_ms"], df["adc_filtrado"])
    plt.xlabel("Tiempo (ms)")
    plt.ylabel("Amplitud")
    plt.title(f"Señal filtrada por el ESP32 - medición {numero}")
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(
        carpeta / f"senal_filtrada_{numero}.png",
        dpi=150
    )
    plt.close()

    plt.figure(figsize=(12, 5))
    plt.plot(df["tiempo_ms"], df["residual"])
    plt.xlabel("Tiempo (ms)")
    plt.ylabel("Amplitud")
    plt.title(
        f"Señal residual: medición - baseline - corrida {numero}"
    )
    plt.grid(True)
    plt.tight_layout()
    plt.savefig(
        carpeta / f"senal_residual_{numero}.png",
        dpi=150
    )
    plt.close()

    plt.figure(figsize=(12, 5))
    plt.plot(
        df["tiempo_ms"],
        df["adc_filtrado"],
        label="Filtrada"
    )
    plt.plot(
        df["tiempo_ms"],
        df["residual"],
        label="Residual"
    )
    plt.xlabel("Tiempo (ms)")
    plt.ylabel("Amplitud")
    plt.title(
        f"Señal filtrada vs residual - medición {numero}"
    )
    plt.grid(True)
    plt.legend()
    plt.tight_layout()
    plt.savefig(
        carpeta / f"comparacion_residual_{numero}.png",
        dpi=150
    )
    plt.close()


def main():
    carpeta, numero = crear_carpeta_salida()

    print()
    print("================================")
    print(f" MEDICION {numero}")
    print("================================")
    print(f"Carpeta de salida: {carpeta}")
    print()

    metricas, crudos, filtrados, residual = ejecutar_medicion()

    print()
    print("================================")
    print(" RESULTADO")
    print("================================")

    print(f"Muestras crudas:     {len(crudos)}")
    print(f"Muestras filtradas:  {len(filtrados)}")
    print(f"Muestras residuales: {len(residual)}")

    if len(crudos) != N:
        print("ERROR: cantidad incorrecta de muestras crudas")
        return

    if len(filtrados) != N:
        print("ERROR: cantidad incorrecta de muestras filtradas")
        return

    if len(residual) != N:
        print("ERROR: cantidad incorrecta de muestras residuales")
        return

    df = guardar_csv(
        crudos,
        filtrados,
        residual,
        carpeta,
        numero
    )

    graficar(
        df,
        carpeta,
        numero
    )

    print()
    print("----------- METRICAS ------------")

    if "rms_residual" in metricas:
        print(
            "RMS residual:",
            metricas["rms_residual"]
        )

    if "pico_residual" in metricas:
        print(
            "Pico residual:",
            metricas["pico_residual"]
        )

    if "relacion_rms" in metricas:
        print(
            "Residual / ruido RMS:",
            metricas["relacion_rms"]
        )

    if "relacion_pico" in metricas:
        print(
            "Residual / ruido pico:",
            metricas["relacion_pico"]
        )

    print()
    print("================================")
    print(f" MEDICION GUARDADA: {numero}")
    print("================================")
    print(f"Carpeta: {carpeta}/")
    print()
    print(f"muestras_{numero}.csv")
    print(f"senal_cruda_{numero}.png")
    print(f"senal_filtrada_{numero}.png")
    print(f"senal_residual_{numero}.png")
    print(f"comparacion_residual_{numero}.png")


if __name__ == "__main__":
    main()