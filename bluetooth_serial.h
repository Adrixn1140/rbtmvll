#ifndef BLUETOOTH_SERIAL_H
#define BLUETOOTH_SERIAL_H

#include <Arduino.h>

void bluetoothSerialInit();
void bluetoothSerialTick();
void bluetoothSerialDescartar();
const char* bluetoothSerialEstado();

#endif
