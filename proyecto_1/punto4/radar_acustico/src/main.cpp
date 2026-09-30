#include <Arduino.h>
#include <Wire.h>

void setup() {
    Serial.begin(115200);
    delay(1000);

    Wire.begin(21, 22);

    Serial.println();
    Serial.println("Escaneando bus I2C...");

    byte encontrados = 0;

    for (byte direccion = 1; direccion < 127; direccion++) {
        Wire.beginTransmission(direccion);
        byte error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("Dispositivo encontrado en 0x");

            if (direccion < 16) {
                Serial.print("0");
            }

            Serial.println(direccion, HEX);
            encontrados++;
        }
    }

    if (encontrados == 0) {
        Serial.println("No se encontraron dispositivos I2C.");
    } else {
        Serial.println("Escaneo terminado.");
    }
}

void loop() {
}