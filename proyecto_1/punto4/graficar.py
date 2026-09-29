import time
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt
import serial


# =====================================================
# CONFIGURACION
# =====================================================

PUERTO = "/dev/ttyUSB0"
BAUD = 115200

N = 2048
ZOOM_MS = 10.0


# =====================================================
# CAPTURA SERIAL
# =====================================================

def capturar():
    crudos = []
    filtrados = []

    fs = 86000.0
    duracion_chirp_ms = 2.0
    tiempo_fir_ms = None
    rms_cruda = None
    rms_filtrada = None

    with serial.Serial(PUERTO, BAUD, timeout=1) as s:
        time.sleep(3.0)
        s.reset_input_buffer()

        print("Enviando x...")
        s.write(b"x\n")
        s.flush()

        print("Esperando medicion...")

        t0 = time.time()

        # =================================================
        # ESPERAR INICIO
        # =================================================

        while True:
            linea = s.readline().decode(errors="ignore").strip()

            if linea:
                print("ESP32 >", linea)

            if linea == "----------- MEDICION -----------":
                break

            if time.time() - t0 > 10:
                raise RuntimeError("Timeout esperando inicio de medicion")

        # =================================================
        # LEER CABECERA HASTA CRUDOS
        # =================================================

        while True:
            linea = s.readline().decode(errors="ignore").strip()

            if not linea:
                continue

            print("ESP32 >", linea)

            if linea.startswith("Fs objetivo:"):
                texto = linea.split(":", 1)[1].replace("Hz", "").strip()

                try:
                    fs = float(texto)
                except ValueError:
                    pass

            elif linea.startswith("Duracion chirp:"):
                texto = linea.split(":", 1)[1].replace("ms", "").strip()

                try:
                    duracion_chirp_ms = float(texto)
                except ValueError:
                    pass

            elif linea.startswith("RMS cruda:"):
                texto = linea.split(":", 1)[1].strip()

                try:
                    rms_cruda = float(texto)
                except ValueError:
                    pass

            elif linea.startswith("RMS filtrada:"):
                texto = linea.split(":", 1)[1].strip()

                try:
                    rms_filtrada = float(texto)
                except ValueError:
                    pass

            elif linea.startswith("Tiempo FIR:"):
                texto = linea.split(":", 1)[1].replace("ms", "").strip()

                try:
                    tiempo_fir_ms = float(texto)
                except ValueError:
                    pass

            if linea == "----------- CRUDOS --------------":
                break

        # =================================================
        # LEER CRUDOS
        # =================================================

        while True:
            linea = s.readline().decode(errors="ignore").strip()

            if linea == "----------- FILTRADOS -----------":
                break

            if linea:
                try:
                    crudos.append(float(linea))
                except ValueError:
                    pass

        # =================================================
        # LEER FILTRADOS
        # =================================================

        while True:
            linea = s.readline().decode(errors="ignore").strip()

            if linea == "------------- FIN ---------------":
                break

            if linea:
                try:
                    filtrados.append(float(linea))
                except ValueError:
                    pass

    return (
        np.array(crudos, dtype=float),
        np.array(filtrados, dtype=float),
        fs,
        duracion_chirp_ms,
        rms_cruda,
        rms_filtrada,
        tiempo_fir_ms
    )


# =====================================================
# CAPTURAR
# =====================================================

crudos, filtrados, FS, duracion_chirp_ms, rms_cruda, rms_filtrada, tiempo_fir_ms = capturar()


# =====================================================
# VALIDAR
# =====================================================

print()
print("===== DATOS RECIBIDOS =====")
print(f"Muestras crudas: {len(crudos)}")
print(f"Muestras filtradas: {len(filtrados)}")
print(f"Fs: {FS:.1f} Hz")

if len(crudos) != N:
    print(f"ADVERTENCIA: esperaba {N} muestras crudas")

if len(filtrados) != N:
    print(f"ADVERTENCIA: esperaba {N} muestras filtradas")

N_USAR = min(len(crudos), len(filtrados))

crudos = crudos[:N_USAR]
filtrados = filtrados[:N_USAR]


# =====================================================
# CENTRAR SEÑAL CRUDA
# =====================================================

media = np.mean(crudos)
cruda_centrada = crudos - media


# =====================================================
# EJES
# =====================================================

n = np.arange(N_USAR)
t_ms = n / FS * 1000.0


# =====================================================
# ESTADISTICAS
# =====================================================

print()
print("===== ADC =====")
print(f"Media cruda: {media:.2f}")
print(f"Min ADC: {np.min(crudos):.0f}")
print(f"Max ADC: {np.max(crudos):.0f}")
print(f"Rango ADC: {np.max(crudos) - np.min(crudos):.0f}")

print()
print("===== FILTRO ESP32 =====")

if rms_cruda is not None:
    print(f"RMS cruda ESP32: {rms_cruda:.2f}")

if rms_filtrada is not None:
    print(f"RMS filtrada ESP32: {rms_filtrada:.2f}")

if tiempo_fir_ms is not None:
    print(f"Tiempo FIR ESP32: {tiempo_fir_ms:.3f} ms")

rms_python_cruda = np.sqrt(np.mean(cruda_centrada ** 2))
rms_python_filtrada = np.sqrt(np.mean(filtrados ** 2))

print(f"RMS cruda verificada: {rms_python_cruda:.2f}")
print(f"RMS filtrada verificada: {rms_python_filtrada:.2f}")


# =====================================================
# GUARDAR CSV
# =====================================================

df = pd.DataFrame({
    "muestra": n,
    "tiempo_ms": t_ms,
    "adc": crudos,
    "adc_centrado": cruda_centrada,
    "adc_filtrado_esp32": filtrados
})

df.to_csv(
    "muestras.csv",
    index=False
)

print()
print("Guardado: muestras.csv")


# =====================================================
# GRAFICA 1: CRUDA COMPLETA
# =====================================================

plt.figure(figsize=(13, 5))

plt.plot(
    t_ms,
    cruda_centrada,
    linewidth=0.8
)

plt.axvline(
    duracion_chirp_ms,
    linestyle="--",
    label=f"Fin chirp {duracion_chirp_ms:.2f} ms"
)

plt.xlabel("Tiempo (ms)")
plt.ylabel("ADC - media")
plt.title("Señal cruda recibida")

plt.grid(True)
plt.legend()
plt.tight_layout()

plt.savefig(
    "senal_cruda.png",
    dpi=150
)


# =====================================================
# GRAFICA 2: FILTRADA COMPLETA
# =====================================================

plt.figure(figsize=(13, 5))

plt.plot(
    t_ms,
    filtrados,
    linewidth=0.8
)

plt.axvline(
    duracion_chirp_ms,
    linestyle="--",
    label=f"Fin chirp {duracion_chirp_ms:.2f} ms"
)

plt.xlabel("Tiempo (ms)")
plt.ylabel("Salida FIR")
plt.title("Señal filtrada por el ESP32 | 3.5-11 kHz")

plt.grid(True)
plt.legend()
plt.tight_layout()

plt.savefig(
    "senal_filtrada_esp32.png",
    dpi=150
)


# =====================================================
# GRAFICA 3: COMPARACION
# =====================================================

plt.figure(figsize=(13, 6))

plt.plot(
    t_ms,
    cruda_centrada,
    linewidth=0.8,
    alpha=0.5,
    label="Cruda"
)

plt.plot(
    t_ms,
    filtrados,
    linewidth=1.0,
    label="FIR ESP32"
)

plt.axvline(
    duracion_chirp_ms,
    linestyle="--",
    label=f"Fin chirp {duracion_chirp_ms:.2f} ms"
)

plt.xlim(0, ZOOM_MS)

plt.xlabel("Tiempo (ms)")
plt.ylabel("Amplitud")
plt.title(f"Cruda vs FIR ESP32 | primeros {ZOOM_MS:.0f} ms")

plt.grid(True)
plt.legend()
plt.tight_layout()

plt.savefig(
    "comparacion_esp32.png",
    dpi=150
)


# =====================================================
# GRAFICA 4: FILTRADA ZOOM
# =====================================================

plt.figure(figsize=(13, 5))

plt.plot(
    t_ms,
    filtrados,
    linewidth=1.0
)

plt.axvline(
    duracion_chirp_ms,
    linestyle="--",
    label=f"Fin chirp {duracion_chirp_ms:.2f} ms"
)

plt.xlim(0, ZOOM_MS)

plt.xlabel("Tiempo (ms)")
plt.ylabel("Salida FIR")
plt.title(f"FIR calculado en ESP32 | primeros {ZOOM_MS:.0f} ms")

plt.grid(True)
plt.legend()
plt.tight_layout()

plt.savefig(
    "senal_filtrada_esp32_zoom.png",
    dpi=150
)


# =====================================================
# FINAL
# =====================================================

print("Guardado: senal_cruda.png")
print("Guardado: senal_filtrada_esp32.png")
print("Guardado: comparacion_esp32.png")
print("Guardado: senal_filtrada_esp32_zoom.png")

plt.show()