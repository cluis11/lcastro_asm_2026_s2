import numpy as np
import matplotlib.pyplot as plt

from senal import generar_chirp, generar_recibida, calcular_distancia, fs
from correlacion import correlacion, retardo_desde_fuga

chirp = generar_chirp()
retardo_real = 1200
recibida = generar_recibida(chirp, [retardo_real], [0.3])
r = correlacion(recibida, chirp)

retardo = retardo_desde_fuga(r, len(chirp))
fuga = np.argmax(r)                     # posicion de la fuga directa

fig, axes = plt.subplots(3, 1, figsize=(11, 9))

# Senal transmitida: el chirp que emite el parlante
axes[0].plot(chirp)
axes[0].set_title('Senal transmitida (chirp de 2 a 8 kHz)')
axes[0].set_xlabel('Muestras')
axes[0].grid(True)

# Senal recibida: fuga directa, eco y ruido superpuestos
axes[1].plot(recibida, linewidth=0.5)
axes[1].set_title('Senal recibida: el eco no es visible a simple vista')
axes[1].set_xlabel('Muestras')
axes[1].grid(True)

# Correlacion: los dos picos quedan localizados
axes[2].plot(r, linewidth=0.7)
# axvline: linea vertical para marcar la posicion de cada pico
axes[2].axvline(fuga, color='green', linestyle='--', alpha=0.5, label=f'Fuga directa ({fuga})')
axes[2].axvline(fuga + retardo, color='red', linestyle='--', alpha=0.5, label=f'Eco ({fuga + retardo})')
axes[2].set_title(f'Correlacion: retardo {retardo} muestras = {calcular_distancia(retardo):.2f} m')
axes[2].set_xlabel('Retardo (muestras)')
axes[2].legend()
axes[2].grid(True)

plt.tight_layout()
plt.savefig('deteccion_eco.png', dpi=150)

print(f"Retardo detectado: {retardo} muestras")
print(f"Distancia:         {calcular_distancia(retardo):.3f} m")
print("Grafica guardada en deteccion_eco.png")