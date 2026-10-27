#ifndef CONTROL_BT_H
#define CONTROL_BT_H

#include <Arduino.h>

void controlBtInit();
void controlBtTick();
bool controlBtConectado();
const char* controlBtNombre();

#endif
