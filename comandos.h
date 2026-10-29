#ifndef COMANDOS_H
#define COMANDOS_H

#include <Arduino.h>

// Procesa comandos de movimiento desde USB o Bluetooth serial.
// eco = true responde al mismo puerto (útil para la app del celular).
void comandosProcesar(Stream& entrada, const char* origen, bool eco = false);
void comandosReset(Stream& entrada);

#endif
