import time
import csv
import numpy as np
import matplotlib.pyplot as plt
import serial

PUERTO = '/dev/ttyUSB0'
BAUD = 115200

N = 2048
M = 15

FS_OBJETIVO = 32000.0
C = 343.0  # velocidad del sonido [m/s]

# -----------------------------------------------------
# Distancia conocida del objeto para esta prueba
# -----------------------------------------------------

DISTANCIA_OBJETO_M = 0.50

# Separacion aproximada entre tweeter y microfono
SEPARACION_TX_RX_M = 0.10

# Zoom que queremos observar
ZOOM_MS = 12.0


def capturar():
    with serial.Serial(PUERTO, BAUD, timeout=1) as s:

        # Al abrir el puerto, el ESP32 puede reiniciarse
        time.sleep(3.0)

        s.reset_input_buffer()

        print("Enviando comando de medicion...")
        s.write(b'x\n')
        s.flush()

        fs_real = None

        # ---------------------------------------------
        # Esperar inicio
        # ---------------------------------------------

        print("Esperando respuesta del ESP32...")

        tiempo_inicio = time.time()

        while True:

            linea = s.readline().decode(
                errors='ignore'
            ).strip()

            if linea:
                print("ESP32 >", linea)

            if linea == '----------- MEDICION -----------':
                break

            if time.time() - tiempo_inicio > 10:
                raise RuntimeError(
                    "Timeout: no se encontro el inicio "
                    "de la medicion"
                )

        # ---------------------------------------------
        # Leer cabecera
        # ---------------------------------------------

        while True:

            linea = s.readline().decode(
                errors='ignore'
            ).strip()

            if not linea:
                continue

            print("ESP32 >", linea)

            if linea.startswith("Fs real:"):

                try:

                    texto = linea.split(
                        ":",
                        1
                    )[1]

                    texto = texto.replace(
                        "Hz",
                        ""
                    ).strip()

                    fs_real = float(texto)

                except ValueError:
                    pass

            if linea == "--------------------------------":
                break

        if fs_real is None:

            print(
                "Advertencia: no se pudo leer Fs real. "
                f"Usando {FS_OBJETIVO} Hz"
            )

            fs_real = FS_OBJETIVO

        # ---------------------------------------------
        # Leer muestras
        # ---------------------------------------------

        muestras = []

        tiempo_inicio = time.time()

        while True:

            linea = s.readline().decode(
                errors='ignore'
            ).strip()

            if linea == '------------- FIN --------------':
                break

            if linea:

                try:
                    muestras.append(
                        int(linea)
                    )

                except ValueError:
                    print(
                        "Linea ignorada:",
                        linea
                    )

            if time.time() - tiempo_inicio > 15:

                raise RuntimeError(
                    "Timeout esperando las muestras"
                )

    return np.array(
        muestras,
        dtype=float
    ), fs_real


# =====================================================
# PROGRAMA PRINCIPAL
# =====================================================

if __name__ == "__main__":

    datos, FS = capturar()

    print()
    print(
        f"Muestras recibidas: {len(datos)}"
    )

    print(
        f"Fs utilizada: {FS:.2f} Hz"
    )

    if len(datos) == 0:
        raise RuntimeError(
            "No se recibieron muestras"
        )

    if len(datos) != N:

        print(
            f"Advertencia: se esperaban {N} muestras "
            f"pero llegaron {len(datos)}"
        )

    # =================================================
    # QUITAR COMPONENTE DC
    # =================================================

    media = np.mean(datos)

    datos_c = datos - media

    print(
        f"Media ADC: {media:.2f}"
    )

    print(
        f"Min centrado: {np.min(datos_c):.2f}"
    )

    print(
        f"Max centrado: {np.max(datos_c):.2f}"
    )

    # =================================================
    # EJES
    # =================================================

    n = np.arange(
        len(datos_c)
    )

    t_s = n / FS

    t_ms = t_s * 1000.0

    d_m = C * t_s / 2.0

    # =================================================
    # CHIRP
    # =================================================

    duracion_chirp_s = M / FS

    zona_ciega_ms = (
        duracion_chirp_s *
        1000.0
    )

    zona_ciega_m = (
        C *
        duracion_chirp_s /
        2.0
    )

    print()

    print(
        f"Duracion chirp: "
        f"{zona_ciega_ms:.3f} ms"
    )

    print(
        f"Zona ciega teorica por duracion: "
        f"{zona_ciega_m:.3f} m"
    )

    # =================================================
    # TIEMPO ESPERADO DEL ECO
    # =================================================
    #
    # Como TX y RX estan separados 10 cm:
    #
    #       objeto
    #         *
    #       /   \
    #      /     \
    #    TX       RX
    #
    # Para objeto centrado:
    #
    # camino total =
    # 2 * sqrt(d^2 + (separacion/2)^2)
    #

    mitad_separacion = (
        SEPARACION_TX_RX_M / 2.0
    )

    camino_total = (
        2.0 *
        np.sqrt(
            DISTANCIA_OBJETO_M ** 2 +
            mitad_separacion ** 2
        )
    )

    tiempo_eco_s = (
        camino_total / C
    )

    tiempo_eco_ms = (
        tiempo_eco_s * 1000.0
    )

    muestra_eco = (
        tiempo_eco_s * FS
    )

    print()

    print(
        f"Objeto de prueba: "
        f"{DISTANCIA_OBJETO_M:.2f} m"
    )

    print(
        f"Separacion TX-RX: "
        f"{SEPARACION_TX_RX_M:.2f} m"
    )

    print(
        f"Camino acustico esperado: "
        f"{camino_total:.4f} m"
    )

    print(
        f"Tiempo esperado del eco: "
        f"{tiempo_eco_ms:.3f} ms"
    )

    print(
        f"Muestra esperada del eco: "
        f"{muestra_eco:.1f}"
    )

    # =================================================
    # GUARDAR CSV
    # =================================================

    with open(
        "muestras.csv",
        "w",
        newline=""
    ) as archivo:

        escritor = csv.writer(
            archivo
        )

        escritor.writerow([
            "muestra",
            "tiempo_ms",
            "distancia_equivalente_m",
            "adc",
            "adc_centrado"
        ])

        for i in range(len(datos)):

            escritor.writerow([
                i,
                t_ms[i],
                d_m[i],
                int(datos[i]),
                datos_c[i]
            ])

    print()
    print(
        "Guardado: muestras.csv"
    )

    # =================================================
    # GRAFICA COMPLETA
    # =================================================

    fig, ax1 = plt.subplots(
        figsize=(14, 6)
    )

    ax1.plot(
        t_ms,
        datos_c,
        linewidth=0.6,
        label='Microfono'
    )

    ax1.set_xlabel(
        'Tiempo desde emision (ms)'
    )

    ax1.set_ylabel(
        'ADC - media'
    )

    ax1.set_title(
        f'Señal cruda del microfono | '
        f'M={M} | Fs={FS:.1f} Hz'
    )

    ax1.grid(True)

    # -------------------------------------------------
    # Eje superior
    # -------------------------------------------------

    ax2 = ax1.secondary_xaxis(
        'top',
        functions=(
            lambda t:
                (t / 1000.0) *
                C / 2.0,

            lambda d:
                (2.0 * d / C) *
                1000.0
        )
    )

    ax2.set_xlabel(
        'Distancia equivalente por '
        'tiempo de vuelo (m)'
    )

    # -------------------------------------------------
    # Duracion chirp
    # -------------------------------------------------

    ax1.axvspan(
        0,
        zona_ciega_ms,
        alpha=0.15,
        label=(
            f'Chirp: '
            f'{zona_ciega_ms:.2f} ms '
            f'(~{zona_ciega_m:.2f} m)'
        )
    )

    ax1.legend()

    plt.tight_layout()

    plt.savefig(
        'senal_cruda.png',
        dpi=150
    )

    print(
        "Guardado: senal_cruda.png"
    )

    # =================================================
    # GRAFICA ZOOM 0 - 12 ms
    # =================================================

    fig_zoom, axz = plt.subplots(
        figsize=(14, 6)
    )

    axz.plot(
        t_ms,
        datos_c,
        marker='.',
        markersize=4,
        linewidth=0.8,
        label='Microfono'
    )

    # Mostrar solamente primeros 12 ms
    axz.set_xlim(
        0,
        ZOOM_MS
    )

    axz.set_xlabel(
        'Tiempo desde emision (ms)'
    )

    axz.set_ylabel(
        'ADC - media'
    )

    axz.set_title(
        f'Zoom primeros {ZOOM_MS:.0f} ms | '
        f'Objeto esperado a '
        f'{DISTANCIA_OBJETO_M:.2f} m'
    )

    axz.grid(True)

    # -------------------------------------------------
    # Zona donde todavia se esta transmitiendo
    # -------------------------------------------------

    axz.axvspan(
        0,
        zona_ciega_ms,
        alpha=0.15,
        label=(
            f'Chirp termina: '
            f'{zona_ciega_ms:.2f} ms'
        )
    )

    # -------------------------------------------------
    # Tiempo esperado del eco del carton
    # -------------------------------------------------

    axz.axvline(
        tiempo_eco_ms,
        linestyle='--',
        linewidth=1.5,
        label=(
            f'Eco esperado 50 cm: '
            f'{tiempo_eco_ms:.2f} ms '
            f'(n~{muestra_eco:.1f})'
        )
    )

    # -------------------------------------------------
    # Eje superior de distancia
    # -------------------------------------------------

    axz2 = axz.secondary_xaxis(
        'top',
        functions=(
            lambda t:
                (t / 1000.0) *
                C / 2.0,

            lambda d:
                (2.0 * d / C) *
                1000.0
        )
    )

    axz2.set_xlabel(
        'Distancia equivalente aproximada (m)'
    )

    axz.legend()

    plt.tight_layout()

    plt.savefig(
        'senal_zoom.png',
        dpi=150
    )

    print(
        "Guardado: senal_zoom.png"
    )

    # =================================================
    # MOSTRAR
    # =================================================

    plt.show()