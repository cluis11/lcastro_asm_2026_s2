import numpy as np
import matplotlib.pyplot as plt

from senal import generar_chirp, generar_recibida, calcular_distancia, fs


# Entrada: x -- senal recibida (larga)
#          h -- senal conocida que se busca (corta)
# Salida: r -- correlacion; r[m] mide el parecido de x con h desplazada m muestras
# Restriccion:  len(h) no debe superar len(x)
def correlacion(x, h):
    N = len(x)
    M = len(h)
    r = np.zeros(N - M + 1) # una posicion por cada desplazamiento
    for m in range(N - M + 1): # m: posicion donde se apoya el patron
        for i in range(M): # i: recorre las M muestras del patron
            r[m] += x[m + i] * h[i] # producto punto a punto y acumulacion
    return r

# Entrada: r -- correlacion
#          M -- largo del chirp
# Salida: retardo en muestras, contado desde la muestra 0
# Restriccion: asume que la captura arranca junto con la emision
def retardo_desde_cero(r, M):
    return M + np.argmax(r[M:])


# Entrada: r -- correlacion
#          M -- largo del chirp
# Salida: retardo en muestras, contado desde la fuga directa
# Restriccion: la fuga debe ser el pico mas alto de la correlacion
def retardo_desde_fuga(r, M):
    fuga = np.argmax(r) # primer pico: fuga directa
    inicio = fuga + M # saltar la zona ciega
    if inicio >= len(r): # no queda senal despues de la zona ciega
        return None 
    eco = inicio + np.argmax(r[inicio:])
    return eco - fuga


if __name__ == "__main__":
    # Ejemplo con numeros pequenos
    x_demo = np.array([0, 0, 1, 2, 1, 0, 0, 0])
    h_demo = np.array([1, 2, 1])
    r_demo = correlacion(x_demo, h_demo)
    print("=== Ejemplo con numeros pequenos ===")
    print("Senal:      ", x_demo)
    print("Patron:     ", h_demo)
    print("Correlacion:", r_demo)
    print("Pico en:    ", np.argmax(r_demo), "(el patron empieza ahi)")

    # Caso real: buscar el eco dentro de la senal recibida
    print("\n=== Deteccion del eco ===")
    chirp = generar_chirp()
    retardo_real = 1200
    recibida = generar_recibida(chirp, [retardo_real], [0.3])

    r = correlacion(recibida, chirp)

    retardo = retardo_desde_fuga(r, len(chirp))
    print(f"Retardo real:      {retardo_real} muestras")
    print(f"Retardo detectado: {retardo} muestras")
    print(f"Distancia:         {calcular_distancia(retardo):.3f} m")

    plt.figure(figsize=(10, 4))
    plt.plot(r)
    plt.xlabel('Retardo (muestras)')
    plt.ylabel('Correlacion')
    plt.title('Correlacion de la senal recibida con el chirp')
    plt.grid(True)
    plt.savefig('correlacion_chirp_largo.png', dpi=150)

    print("\n=== Prueba con distintos desfases de captura ===")
    print(f"{'Desfase':>8} {'Desde cero':>12} {'Desde fuga':>12}")

    for desfase in [0, 100, 350, 800]:
        rec = generar_recibida(chirp, [retardo_real], [0.3], desfase=desfase)
        rr = correlacion(rec, chirp)
        d1 = retardo_desde_cero(rr, len(chirp))
        d2 = retardo_desde_fuga(rr, len(chirp))
        print(f"{desfase:>8} {d1:>12} {d2:>12}")