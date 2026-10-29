#include "modo.h"

#include "config.h"
#include "control_bt.h"
#include "motores.h"
#include "oled.h"
#include "servo_ultrasonido.h"
#include "telegram_bot.h"
#include "wifi_manager.h"

namespace {

ModoOperacion modo = ModoOperacion::AUTOMATICO;

void aplicarModo(ModoOperacion nuevo, bool avisarTelegram) {
  motoresDetener();

  if (nuevo == ModoOperacion::MANUAL) {
    if (avisarTelegram) {
      telegramEnviarMensaje(
          F("🎮 *Modo MANUAL*\n\nWi-Fi se apagará para liberar la radio. "
            "Usa el GameSir: B = datos, Y = volver a automático."));
    }

    controlBtActivar();
    wifiApagar();
    servoUltrasonidoCentrar();

    Serial.println(
        F("[MODO] MANUAL: Wi-Fi OFF | Bluetooth ON | B=datos, Y=auto."));
  } else {
    controlBtDesactivar();
    wifiEncender();
    servoUltrasonidoCentrar();

    Serial.println(
        F("[MODO] AUTOMATICO: Wi-Fi ON | Bluetooth OFF | servo escaneando."));
  }

  modo = nuevo;
  oledMostrarModo(modo);
}

}  // namespace

void modoInit() {
  const ModoOperacion inicial =
      MODO_INICIAL_AUTOMATICO ? ModoOperacion::AUTOMATICO
                              : ModoOperacion::MANUAL;

  modo = inicial;
  aplicarModo(inicial, false);
}

bool modoCambiar(ModoOperacion nuevo) {
  if (nuevo == modo) {
    return false;
  }

  aplicarModo(nuevo, true);
  return true;
}

ModoOperacion modoActual() {
  return modo;
}

const char* modoNombre() {
  return modo == ModoOperacion::MANUAL ? "MANUAL" : "AUTOMATICO";
}

bool modoEsManual() {
  return modo == ModoOperacion::MANUAL;
}

bool modoEsAutomatico() {
  return modo == ModoOperacion::AUTOMATICO;
}
