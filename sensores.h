#ifndef SENSORES_H
#define SENSORES_H

#include <Arduino.h>

// Lecturas ambientales compartidas por el resto de módulos.
struct LecturasAmbientales {
  float temperaturaC = NAN;        // Última temperatura válida
  float humedadPct = NAN;          // Última humedad válida
  int gasAdc = 0;                  // Promedio de MUESTRAS_GAS (0-4095)
  int gasAdcCrudo = 0;             // Última muestra individual
  bool flamaDetectada = false;     // true cuando el DO está en LOW
  bool dhtValida = false;          // false si la última lectura DHT falló
  unsigned long ultimaActualizacionMs = 0;
};

void sensoresInit();
bool sensoresTick();                            // true si hubo una nueva lectura
const LecturasAmbientales& sensoresLecturas();

#endif
