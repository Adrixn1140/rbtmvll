#ifndef MODO_H
#define MODO_H

#include <Arduino.h>

enum class ModoOperacion : uint8_t {
  MANUAL,
  AUTOMATICO,
};

void modoInit();
bool modoCambiar(ModoOperacion nuevo);
ModoOperacion modoActual();
const char* modoNombre();
bool modoEsManual();
bool modoEsAutomatico();

#endif
