#ifndef BLUETOOTH_SERIAL_H
#define BLUETOOTH_SERIAL_H

#include <Arduino.h>

void bluetoothSerialInit();
void bluetoothSerialTick();
const char* bluetoothSerialEstado();

#endif
