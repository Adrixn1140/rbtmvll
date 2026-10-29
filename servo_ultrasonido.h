#ifndef SERVO_ULTRASONIDO_H
#define SERVO_ULTRASONIDO_H

#include <Arduino.h>

// Servo SG90 como cuello del HC-SR04.
// Índices de distancia: 0 = izquierda, 1 = frente, 2 = derecha.
void servoUltrasonidoInit();
void servoUltrasonidoTick(bool escanear);
void servoUltrasonidoCentrar();

float servoDistanciaCm(uint8_t indice);
float servoDistanciaFrenteCm();
float servoDistanciaIzquierdaCm();
float servoDistanciaDerechaCm();
bool servoObstaculoFrente();
uint8_t servoAnguloActual();
uint8_t servoIndiceActual();
bool servoEscanerActivo();
const char* servoDireccionMasLibre();

#endif
