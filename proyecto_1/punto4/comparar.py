import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# =====================================================
# CONFIGURACION ACTUAL
# =====================================================

FS = 31866.6
C = 343.0

# Separacion TX-RX
SEP = 0.10

# Rango que queremos buscar
D_MIN = 0.50
D_MAX = 0.70

ARCHIVO_CON = "con_carton.csv"
ARCHIVO_SIN = "sin_carton.csv"


# =====================================================
# DISTANCIA BIESTATICA -> TIEMPO
# =====================================================

def distancia_a_tiempo(d):
    """
    Objeto centrado entre TX y RX.

    Camino total:
        2 * sqrt(d^2 + (SEP/2)^2)
    """

    camino = 2.0 * np.sqrt(
        d**2 + (SEP / 2.0)**2
    )

    return camino / C


# =====================================================
# TIEMPO -> DISTANCIA BIESTATICA
# =====================================================

def tiempo_a_distancia(t):
    """
    Inversa de la geometria biestatica.
    """

    mitad_camino = C * t / 2.0

    valor = (
        mitad_camino**2 -
        (SEP / 2.0)**2
    )

    if valor <= 0:
        return 0.0

    return np.sqrt(valor)


# =====================================================
# CARGAR DATOS
# =====================================================

con = pd.read_csv(ARCHIVO_CON)
sin = pd.read_csv(ARCHIVO_SIN)

x_con = con["adc"].to_numpy(dtype=float)
x_sin = sin["adc"].to_numpy(dtype=float)

N = min(
    len(x_con),
    len(x_sin)
)

x_con = x_con[:N]
x_sin = x_sin[:N]


# =====================================================
# QUITAR DC POR SEPARADO
# =====================================================

x_con -= np.mean(x_con)
x_sin -= np.mean(x_sin)


# =====================================================
# EJES
# =====================================================

n = np.arange(N)

t_s = n / FS
t_ms = t_s * 1000.0

distancia = np.array([
    tiempo_a_distancia(t)
    for t in t_s
])


# =====================================================
# RESTA CON - SIN
# =====================================================

diferencia = x_con - x_sin


# =====================================================
# VENTANA 50-70 cm
# =====================================================

t_min = distancia_a_tiempo(D_MIN)
t_max = distancia_a_tiempo(D_MAX)

n_min = int(
    np.floor(t_min * FS)
)

n_max = int(
    np.ceil(t_max * FS)
)

print()
print("===== RANGO DE BUSQUEDA =====")

print(
    f"Distancia: "
    f"{D_MIN:.2f} -> {D_MAX:.2f} m"
)

print(
    f"Tiempo: "
    f"{t_min*1000:.3f} -> "
    f"{t_max*1000:.3f} ms"
)

print(
    f"Muestras: "
    f"{n_min} -> {n_max}"
)


# =====================================================
# MAXIMA DIFERENCIA EN EL RANGO
# =====================================================

ventana = diferencia[
    n_min:n_max + 1
]

indice_local = np.argmax(
    np.abs(ventana)
)

indice_pico = (
    n_min +
    indice_local
)

pico = diferencia[
    indice_pico
]

tiempo_pico_s = (
    indice_pico /
    FS
)

tiempo_pico_ms = (
    tiempo_pico_s *
    1000.0
)

distancia_pico = (
    tiempo_a_distancia(
        tiempo_pico_s
    )
)


print()
print("===== RESULTADO =====")

print(
    f"Muestra maxima diferencia: "
    f"{indice_pico}"
)

print(
    f"Tiempo: "
    f"{tiempo_pico_ms:.3f} ms"
)

print(
    f"Diferencia ADC: "
    f"{pico:.2f}"
)

print(
    f"Distancia equivalente: "
    f"{distancia_pico:.3f} m"
)


# =====================================================
# GRAFICA 1
# CON CARTON VS SIN CARTON
# =====================================================

plt.figure(
    figsize=(12, 5)
)

plt.plot(
    distancia,
    x_con,
    label="Con carton",
    linewidth=1.0
)

plt.plot(
    distancia,
    x_sin,
    label="Sin carton",
    linewidth=1.0
)

plt.xlim(
    D_MIN,
    D_MAX
)

plt.xlabel(
    "Distancia al objeto (m)"
)

plt.ylabel(
    "ADC - media"
)

plt.title(
    "Comparacion con/sin carton | "
    "rango 50-70 cm"
)

plt.grid(True)

plt.legend()

plt.tight_layout()

plt.savefig(
    "comparacion_50_70cm.png",
    dpi=150
)


# =====================================================
# GRAFICA 2
# DIFERENCIA
# =====================================================

plt.figure(
    figsize=(12, 5)
)

plt.plot(
    distancia,
    diferencia,
    linewidth=1.0,
    label="Con - Sin"
)

plt.axvline(
    distancia_pico,
    linestyle="--",
    label=(
        f"Max: "
        f"{distancia_pico:.3f} m"
    )
)

plt.xlim(
    D_MIN,
    D_MAX
)

plt.xlabel(
    "Distancia al objeto (m)"
)

plt.ylabel(
    "Diferencia ADC"
)

plt.title(
    "Diferencia con carton - sin carton"
)

plt.grid(True)

plt.legend()

plt.tight_layout()

plt.savefig(
    "diferencia_50_70cm.png",
    dpi=150
)


# =====================================================
# GRAFICA 3
# DIFERENCIA VS TIEMPO
# =====================================================

plt.figure(
    figsize=(12, 5)
)

plt.plot(
    t_ms,
    diferencia,
    linewidth=1.0
)

plt.axvline(
    tiempo_pico_ms,
    linestyle="--",
    label=(
        f"{tiempo_pico_ms:.3f} ms "
        f"= {distancia_pico:.3f} m"
    )
)

plt.xlim(
    t_min * 1000.0,
    t_max * 1000.0
)

plt.xlabel(
    "Tiempo desde emision (ms)"
)

plt.ylabel(
    "Diferencia ADC"
)

plt.title(
    "Diferencia en ventana esperada "
    "50-70 cm"
)

plt.grid(True)

plt.legend()

plt.tight_layout()

plt.savefig(
    "diferencia_tiempo_50_70cm.png",
    dpi=150
)


print()
print(
    "Guardado: comparacion_50_70cm.png"
)

print(
    "Guardado: diferencia_50_70cm.png"
)

print(
    "Guardado: diferencia_tiempo_50_70cm.png"
)

plt.show()