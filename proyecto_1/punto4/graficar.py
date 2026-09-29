import time

import numpy as np
import matplotlib.pyplot as plt
import serial


# =====================================================
# CONFIGURACION
# =====================================================

PUERTO = "/dev/ttyUSB0"
BAUD = 115200

N = 2048

C = 343.0

ZOOM_MS = 10.0


# =====================================================
# CAPTURAR
# =====================================================

def capturar():

    fs_objetivo = None
    fs_real = None

    M = None
    duracion_ms = None
    amplitud = None

    muestras = []

    with serial.Serial(
        PUERTO,
        BAUD,
        timeout=1
    ) as s:

        # El ESP32 puede reiniciarse
        # al abrir el puerto
        time.sleep(3.0)

        s.reset_input_buffer()

        print(
            "Enviando x..."
        )

        s.write(
            b"x\n"
        )

        s.flush()

        print(
            "Esperando medicion..."
        )

        # =================================================
        # ESPERAR INICIO
        # =================================================

        t0 = time.time()

        while True:

            linea = (
                s.readline()
                .decode(
                    errors="ignore"
                )
                .strip()
            )

            if linea:
                print(
                    "ESP32 >",
                    linea
                )

            if linea == (
                "----------- MEDICION -----------"
            ):
                break

            if (
                time.time()
                - t0
                > 10
            ):
                raise RuntimeError(
                    "Timeout esperando inicio "
                    "de medicion"
                )

        # =================================================
        # LEER CABECERA
        # =================================================

        while True:

            linea = (
                s.readline()
                .decode(
                    errors="ignore"
                )
                .strip()
            )

            if not linea:
                continue

            print(
                "ESP32 >",
                linea
            )

            # -----------------------------------------
            # FS OBJETIVO
            # -----------------------------------------

            if linea.startswith(
                "Fs objetivo:"
            ):

                texto = (
                    linea
                    .split(":", 1)[1]
                    .replace(
                        "Hz",
                        ""
                    )
                    .strip()
                )

                try:
                    fs_objetivo = float(
                        texto
                    )
                except ValueError:
                    pass

            # -----------------------------------------
            # FS REAL
            # -----------------------------------------

            elif linea.startswith(
                "Fs real:"
            ):

                texto = (
                    linea
                    .split(":", 1)[1]
                    .replace(
                        "Hz",
                        ""
                    )
                    .strip()
                )

                try:
                    fs_real = float(
                        texto
                    )
                except ValueError:
                    pass

            # -----------------------------------------
            # M
            # -----------------------------------------

            elif linea.startswith(
                "M:"
            ):

                texto = (
                    linea
                    .split(":", 1)[1]
                    .replace(
                        "muestras",
                        ""
                    )
                    .strip()
                )

                try:
                    M = int(
                        texto
                    )
                except ValueError:
                    pass

            # -----------------------------------------
            # DURACION
            # -----------------------------------------

            elif linea.startswith(
                "Duracion:"
            ):

                texto = (
                    linea
                    .split(":", 1)[1]
                    .replace(
                        "ms",
                        ""
                    )
                    .strip()
                )

                try:
                    duracion_ms = float(
                        texto
                    )
                except ValueError:
                    pass

            # -----------------------------------------
            # AMPLITUD
            # -----------------------------------------

            elif linea.startswith(
                "Amplitud:"
            ):

                texto = (
                    linea
                    .split(":", 1)[1]
                    .strip()
                )

                try:
                    amplitud = float(
                        texto
                    )
                except ValueError:
                    pass

            # -----------------------------------------
            # FIN CABECERA
            # -----------------------------------------

            if linea == (
                "--------------------------------"
            ):
                break

        # =================================================
        # LEER MUESTRAS
        # =================================================

        t0 = time.time()

        while True:

            linea = (
                s.readline()
                .decode(
                    errors="ignore"
                )
                .strip()
            )

            if linea == (
                "------------- FIN --------------"
            ):
                break

            if linea:

                try:
                    muestras.append(
                        int(linea)
                    )

                except ValueError:
                    pass

            if (
                time.time()
                - t0
                > 15
            ):
                raise RuntimeError(
                    "Timeout esperando muestras"
                )

    # =================================================
    # VALIDACIONES
    # =================================================

    if len(muestras) == 0:

        raise RuntimeError(
            "No llegaron muestras"
        )

    if fs_objetivo is None:
        fs_objetivo = 86000.0

    if fs_real is None:
        fs_real = fs_objetivo

    if M is None:
        M = 172

    if duracion_ms is None:
        duracion_ms = (
            M /
            fs_objetivo *
            1000.0
        )

    return (
        np.array(
            muestras,
            dtype=float
        ),
        fs_objetivo,
        fs_real,
        M,
        duracion_ms,
        amplitud
    )


# =====================================================
# MAIN
# =====================================================

datos, fs_objetivo, fs_real, M, duracion_ms, amplitud = (
    capturar()
)

print()

print(
    f"Muestras recibidas: "
    f"{len(datos)}"
)

print(
    f"Fs objetivo: "
    f"{fs_objetivo:.2f} Hz"
)

print(
    f"Fs reportada: "
    f"{fs_real:.2f} Hz"
)

print(
    f"M: {M}"
)

print(
    f"Duracion chirp: "
    f"{duracion_ms:.3f} ms"
)

if amplitud is not None:

    print(
        f"Amplitud chirp: "
        f"{amplitud:.1f}"
    )


# =====================================================
# CENTRAR
# =====================================================

media = np.mean(
    datos
)

centrado = (
    datos - media
)

print()

print(
    f"Media ADC: "
    f"{media:.2f}"
)

print(
    f"Min ADC: "
    f"{np.min(datos):.0f}"
)

print(
    f"Max ADC: "
    f"{np.max(datos):.0f}"
)

print(
    f"Rango ADC: "
    f"{np.max(datos)-np.min(datos):.0f}"
)

print(
    f"Min centrado: "
    f"{np.min(centrado):.2f}"
)

print(
    f"Max centrado: "
    f"{np.max(centrado):.2f}"
)


# =====================================================
# EJES
# =====================================================

# Para la grafica usamos Fs objetivo.
#
# El "Fs real" calculado por el firmware
# incluye tiempos de lectura/DMA y no es
# una medida perfecta del reloj ADC.

FS = fs_objetivo

n = np.arange(
    len(datos)
)

t_s = (
    n / FS
)

t_ms = (
    t_s * 1000.0
)

distancia_equivalente = (
    C *
    t_s /
    2.0
)


# =====================================================
# ZONA CIEGA TEORICA
# =====================================================

duracion_s = (
    duracion_ms /
    1000.0
)

zona_ciega = (
    C *
    duracion_s /
    2.0
)

print()

print(
    f"Zona ciega teorica: "
    f"{zona_ciega:.3f} m"
)


# =====================================================
# GUARDAR CSV
# =====================================================

tabla = np.column_stack(
    (
        n,
        t_ms,
        datos,
        centrado
    )
)

np.savetxt(
    "muestras.csv",
    tabla,
    delimiter=",",
    header=(
        "muestra,"
        "tiempo_ms,"
        "adc,"
        "adc_centrado"
    ),
    comments="",
    fmt=[
        "%d",
        "%.6f",
        "%.0f",
        "%.6f"
    ]
)

print(
    "Guardado: muestras.csv"
)


# =====================================================
# GRAFICA COMPLETA
# =====================================================

plt.figure(
    figsize=(13, 5)
)

plt.plot(
    t_ms,
    centrado,
    linewidth=0.7
)

plt.axvline(
    duracion_ms,
    linestyle="--",
    label=(
        f"Fin chirp "
        f"{duracion_ms:.2f} ms"
    )
)

plt.xlabel(
    "Tiempo (ms)"
)

plt.ylabel(
    "ADC - media"
)

plt.title(
    "Señal recibida completa"
)

plt.grid(True)

plt.legend()

plt.tight_layout()

plt.savefig(
    "senal_cruda.png",
    dpi=150
)

print(
    "Guardado: senal_cruda.png"
)


# =====================================================
# GRAFICA ZOOM INICIAL
# =====================================================

plt.figure(
    figsize=(13, 5)
)

plt.plot(
    t_ms,
    centrado,
    linewidth=0.8
)

plt.axvline(
    duracion_ms,
    linestyle="--",
    label=(
        f"Fin chirp "
        f"{duracion_ms:.2f} ms"
    )
)

plt.xlim(
    0,
    ZOOM_MS
)

plt.xlabel(
    "Tiempo (ms)"
)

plt.ylabel(
    "ADC - media"
)

plt.title(
    f"Zoom primeros "
    f"{ZOOM_MS:.0f} ms"
)

plt.grid(True)

plt.legend()

plt.tight_layout()

plt.savefig(
    "senal_zoom.png",
    dpi=150
)

print(
    "Guardado: senal_zoom.png"
)


# =====================================================
# MOSTRAR
# =====================================================

plt.show()