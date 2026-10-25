#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <Arduino.h>

#include "sensores.h"

void telegramInit();
void telegramTick(const LecturasAmbientales& lecturas);
bool telegramEnviarMensaje(const String& texto);

#endif
