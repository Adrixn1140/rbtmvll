#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

void wifiInit();
void wifiTick();          // Debe llamarse en cada iteración del loop()
void wifiApagar();
void wifiEncender();
bool wifiHabilitado();
bool wifiConectado();
String wifiIp();
int wifiRssi();

#endif
