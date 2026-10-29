#ifndef CONTROL_BT_H
#define CONTROL_BT_H

#include <Arduino.h>

void controlBtInit();
void controlBtTick();
void controlBtActivar();
void controlBtDesactivar();
bool controlBtHabilitado();
void controlBtImprimirEstado();
void controlBtOlvidarClaves();
void controlBtReiniciarBusqueda();
void controlBtAlternarDebug();
bool controlBtConectado();
const char* controlBtNombre();
bool controlBtConsumirSolicitudDatos();
bool controlBtConsumirSolicitudAuto();

#endif
