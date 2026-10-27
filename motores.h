#ifndef MOTORES_H
#define MOTORES_H

#include <Arduino.h>

enum class DireccionMovimiento : uint8_t {
  DETENIDO = 0,
  ADELANTE,
  ATRAS,
  IZQUIERDA,
  DERECHA
};

void motoresInit();
void motoresTick();
void motoresMover(DireccionMovimiento direccion, unsigned long duracionMs);
void motoresControlContinuo(DireccionMovimiento direccion);
void motoresDetener();
void motoresDemo();
bool motoresDemoActiva();
const char* motoresEstadoTexto();

#endif
