import time
from datetime import datetime

import numpy as np
import matplotlib.pyplot as plt
import serial


# =====================================================
# SERIAL
# =====================================================

PUERTO = "/dev/ttyUSB1"
BAUD = 115200

N = 2048


# =====================================================
# PARAMETROS
# =====================================================

FS_NOMINAL = 32000.0

C = 343.0

# Separacion TX-RX
SEP = 0.10

# Longitud de la firma real que tomamos como referencia
L_REF = 48

# Buscar la respuesta inicial dentro de los primeros ms
BUSQUEDA_INICIAL_MS = 6.0

# Ignorar algo de ringing después de la referencia
MARGEN_EXTRA = 32


# =====================================================
# CAPTURA SERIAL
# =====================================================

def capturar():

    with serial.Serial(
        PUERTO,
        BAUD,
        timeout=1
    ) as s:

        time.sleep(3.0)

        s.reset_input_buffer()

        print("Enviando comando de medicion...")

        s.write(b"x\n")
        s.flush()

        muestras = []
        fs_real = None

        print("Esperando respuesta del ESP32...")

        t0 = time.time()

        # ---------------------------------------------
        # ESPERAR INICIO
        # ---------------------------------------------

        while True:

            linea = (
                s.readline()
                .decode(errors="ignore")
                .strip()
            )

            if linea:
                print("ESP32 >", linea)

            if linea == "----------- MEDICION -----------":
                break

            if time.time() - t0 > 10:
                raise RuntimeError(
                    "Timeout esperando inicio de medicion"
                )

        # ---------------------------------------------
        # LEER CABECERA
        # ---------------------------------------------

        while True:

            linea = (
                s.readline()
                .decode(errors="ignore")
                .strip()
            )

            if not linea:
                continue

            print("ESP32 >", linea)

            if linea.startswith("Fs real:"):

                try:

                    texto = (
                        linea
                        .split(":", 1)[1]
                        .replace("Hz", "")
                        .strip()
                    )

                    fs_real = float(texto)

                except ValueError:
                    pass

            if linea == "--------------------------------":
                break

        if fs_real is None:

            print(
                "No se pudo leer Fs real, "
                "usando 32000 Hz"
            )

            fs_real = FS_NOMINAL

        # ---------------------------------------------
        # LEER MUESTRAS
        # ---------------------------------------------

        t0 = time.time()

        while True:

            linea = (
                s.readline()
                .decode(errors="ignore")
                .strip()
            )

            if linea == "------------- FIN --------------":
                break

            if linea:

                try:
                    muestras.append(
                        int(linea)
                    )

                except ValueError:
                    pass

            if time.time() - t0 > 15:

                raise RuntimeError(
                    "Timeout esperando muestras"
                )

    return (
        np.array(
            muestras,
            dtype=float
        ),
        fs_real
    )


# =====================================================
# CORRELACION NORMALIZADA
# =====================================================

def correlacion_normalizada(
    senal,
    referencia
):

    senal = np.asarray(
        senal,
        dtype=float
    )

    referencia = np.asarray(
        referencia,
        dtype=float
    )

    L = len(referencia)

    referencia = (
        referencia
        - np.mean(referencia)
    )

    energia_ref = np.sum(
        referencia ** 2
    )

    if energia_ref <= 0:
        raise RuntimeError(
            "Referencia sin energia"
        )

    # ---------------------------------------------
    # NUMERADOR
    # ---------------------------------------------

    numerador = np.correlate(
        senal,
        referencia,
        mode="valid"
    )

    # ---------------------------------------------
    # ENERGIA LOCAL
    # ---------------------------------------------

    senal2 = (
        senal ** 2
    )

    acumulada = np.concatenate(
        (
            [0.0],
            np.cumsum(senal2)
        )
    )

    energia_local = (
        acumulada[L:]
        - acumulada[:-L]
    )

    denominador = np.sqrt(
        energia_local
        * energia_ref
    )

    corr = np.zeros_like(
        numerador
    )

    validos = (
        denominador > 1e-12
    )

    corr[validos] = (
        numerador[validos]
        / denominador[validos]
    )

    return corr


# =====================================================
# GEOMETRIA
# =====================================================

def camino_a_distancia(
    camino_total
):

    valor = (
        (camino_total / 2.0) ** 2
        - (SEP / 2.0) ** 2
    )

    if valor <= 0:
        return 0.0

    return np.sqrt(valor)


# =====================================================
# MAIN
# =====================================================

if __name__ == "__main__":

    # =================================================
    # CAPTURA
    # =================================================

    datos, FS = capturar()

    if len(datos) == 0:

        raise RuntimeError(
            "No llegaron muestras"
        )

    print()
    print("==============================")
    print(" CAPTURA")
    print("==============================")

    print(
        f"Muestras: {len(datos)}"
    )

    print(
        f"Fs: {FS:.2f} Hz"
    )

    media = np.mean(
        datos
    )

    senal = (
        datos - media
    )

    print(
        f"Media ADC: {media:.2f}"
    )

    print(
        f"Min ADC: {np.min(datos):.0f}"
    )

    print(
        f"Max ADC: {np.max(datos):.0f}"
    )

    print(
        f"Rango ADC: "
        f"{np.max(datos) - np.min(datos):.0f}"
    )

    # =================================================
    # GUARDAR CSV
    # =================================================

    timestamp = datetime.now().strftime(
        "%Y%m%d_%H%M%S"
    )

    nombre_csv = (
        f"captura_{timestamp}.csv"
    )

    tiempo_ms = (
        np.arange(len(datos))
        / FS
        * 1000.0
    )

    tabla = np.column_stack(
        (
            np.arange(len(datos)),
            tiempo_ms,
            datos,
            senal
        )
    )

    np.savetxt(
        nombre_csv,
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

    print()
    print(
        f"Guardado: {nombre_csv}"
    )

    # =================================================
    # BUSCAR RESPUESTA INICIAL REAL
    # =================================================

    limite = int(
        BUSQUEDA_INICIAL_MS
        / 1000.0
        * FS
    )

    limite = min(
        limite,
        len(senal)
    )

    if limite <= L_REF:

        raise RuntimeError(
            "Ventana inicial demasiado corta"
        )

    # Energia movil
    energia = np.convolve(
        senal[:limite] ** 2,
        np.ones(L_REF),
        mode="valid"
    )

    inicio_ref = int(
        np.argmax(energia)
    )

    fin_ref = (
        inicio_ref + L_REF
    )

    referencia = senal[
        inicio_ref:fin_ref
    ].copy()

    referencia -= np.mean(
        referencia
    )

    print()
    print("==============================")
    print(" REFERENCIA REAL")
    print("==============================")

    print(
        f"Inicio referencia: {inicio_ref}"
    )

    print(
        f"Fin referencia: {fin_ref - 1}"
    )

    print(
        f"Tiempo inicio: "
        f"{inicio_ref / FS * 1000.0:.3f} ms"
    )

    print(
        f"Longitud: {L_REF} muestras"
    )

    # =================================================
    # CORRELACION
    # =================================================

    corr = correlacion_normalizada(
        senal,
        referencia
    )

    env = np.abs(
        corr
    )

    # =================================================
    # LA REFERENCIA MISMA DEBE DAR ~1
    # =================================================

    valor_ref = env[
        inicio_ref
    ]

    print()
    print("==============================")
    print(" VALIDACION REFERENCIA")
    print("==============================")

    print(
        f"Correlacion en referencia: "
        f"{valor_ref:.4f}"
    )

    if valor_ref < 0.90:

        print(
            "ADVERTENCIA: la referencia no "
            "esta correlacionando cerca de 1."
        )

    # =================================================
    # IGNORAR INICIO
    # =================================================

    inicio_busqueda = (
        fin_ref
        + MARGEN_EXTRA
    )

    if inicio_busqueda >= len(env):

        raise RuntimeError(
            "No queda señal util para buscar eco"
        )

    print()
    print("==============================")
    print(" ZONA IGNORADA")
    print("==============================")

    print(
        f"Hasta muestra: "
        f"{inicio_busqueda - 1}"
    )

    print(
        f"Busqueda inicia en: "
        f"{inicio_busqueda}"
    )

    print(
        f"Retardo minimo relativo: "
        f"{(inicio_busqueda - inicio_ref) / FS * 1000.0:.3f} ms"
    )

    # =================================================
    # BUSCAR ECO
    # =================================================

    zona = env[
        inicio_busqueda:
    ]

    eco = (
        inicio_busqueda
        + int(
            np.argmax(zona)
        )
    )

    valor_eco = env[
        eco
    ]

    fondo = float(
        np.median(zona)
    )

    if fondo > 0:

        razon = (
            valor_eco
            / fondo
        )

    else:

        razon = np.inf

    # =================================================
    # RETARDO
    # =================================================

    retardo_muestras = (
        eco - inicio_ref
    )

    retardo_s = (
        retardo_muestras
        / FS
    )

    # =================================================
    # DISTANCIA PRELIMINAR
    # =================================================

    camino_directo = SEP

    camino_eco = (
        camino_directo
        + C * retardo_s
    )

    distancia = camino_a_distancia(
        camino_eco
    )

    print()
    print("==============================")
    print(" ECO CANDIDATO")
    print("==============================")

    print(
        f"Muestra eco: {eco}"
    )

    print(
        f"Correlacion eco: "
        f"{valor_eco:.4f}"
    )

    print(
        f"Fondo: "
        f"{fondo:.4f}"
    )

    print(
        f"Razon pico/fondo: "
        f"{razon:.2f}"
    )

    print(
        f"Retardo: "
        f"{retardo_muestras} muestras"
    )

    print(
        f"Retardo: "
        f"{retardo_s * 1000.0:.3f} ms"
    )

    print()

    print(
        f"DISTANCIA PRELIMINAR: "
        f"{distancia:.3f} m"
    )

    # =================================================
    # GRAFICA SEÑAL
    # =================================================

    plt.figure(
        figsize=(13, 5)
    )

    plt.plot(
        tiempo_ms,
        senal,
        linewidth=0.8
    )

    plt.axvspan(
        inicio_ref / FS * 1000.0,
        fin_ref / FS * 1000.0,
        alpha=0.2,
        label="Referencia real"
    )

    plt.xlabel(
        "Tiempo (ms)"
    )

    plt.ylabel(
        "ADC - media"
    )

    plt.title(
        "Señal recibida"
    )

    plt.grid(True)
    plt.legend()

    plt.tight_layout()

    nombre_senal = (
        f"senal_{timestamp}.png"
    )

    plt.savefig(
        nombre_senal,
        dpi=150
    )

    # =================================================
    # GRAFICA CORRELACION
    # =================================================

    eje_corr_ms = (
        np.arange(len(env))
        / FS
        * 1000.0
    )

    plt.figure(
        figsize=(13, 5)
    )

    plt.plot(
        eje_corr_ms,
        env,
        linewidth=0.8,
        label="|Correlacion|"
    )

    plt.axvline(
        inicio_ref / FS * 1000.0,
        linestyle="--",
        label="Referencia"
    )

    plt.axvline(
        inicio_busqueda / FS * 1000.0,
        linestyle=":",
        label="Inicio busqueda"
    )

    plt.axvline(
        eco / FS * 1000.0,
        linestyle="--",
        label=(
            f"Eco candidato "
            f"{distancia:.2f} m"
        )
    )

    plt.xlabel(
        "Tiempo / desplazamiento (ms)"
    )

    plt.ylabel(
        "|Correlacion normalizada|"
    )

    plt.title(
        "Correlacion usando respuesta real"
    )

    plt.grid(True)
    plt.legend()

    plt.tight_layout()

    nombre_corr = (
        f"correlacion_{timestamp}.png"
    )

    plt.savefig(
        nombre_corr,
        dpi=150
    )

    print()
    print(
        f"Guardado: {nombre_senal}"
    )

    print(
        f"Guardado: {nombre_corr}"
    )

    plt.show()