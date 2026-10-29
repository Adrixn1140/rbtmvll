#ifndef OLED_H
#define OLED_H

#include <Arduino.h>

#include "modo.h"
#include "sensores.h"

void oledInit();
void oledTick(const LecturasAmbientales& lecturas);
void oledMostrarDatos();
void oledMostrarModo(ModoOperacion modo);
bool oledDisponible();

#endif
