#include <Arduino.h>

#include "config.h"
#include "sensores.h"
#include "wifi_manager.h"
#include "telegram_bot.h"
#include "alertas.h"
#include "motores.h"
#include "control_bt.h"

namespace {

void procesarComandoSerial() {
  while (Serial.available() > 0) {
    const char comando = static_cast<char>(Serial.read());
    switch (comando) {
      case 'a':
      case 'A':
        motoresMover(DireccionMovimiento::ADELANTE,
                     DURACION_MOVIMIENTO_MANUAL_MS);
        break;
      case 'r':
      case 'R':
        motoresMover(DireccionMovimiento::ATRAS,
                     DURACION_MOVIMIENTO_MANUAL_MS);
        break;
      case 'i':
      case 'I':
        motoresMover(DireccionMovimiento::IZQUIERDA,
                     DURACION_MOVIMIENTO_MANUAL_MS);
        break;
      case 'd':
      case 'D':
        motoresMover(DireccionMovimiento::DERECHA,
                     DURACION_MOVIMIENTO_MANUAL_MS);
        break;
      case 's':
      case 'S':
        motoresDetener();
        break;
      case 'p':
      case 'P':
        motoresDemo();
        break;
      default:
        break;
    }
  }
}

void imprimirLecturas(const LecturasAmbientales& lecturas) {
  if (lecturas.dhtValida) {
    Serial.printf(
        "[SENSOR] T=%.1f C | H=%.1f %% | GAS=%d (crudo=%d) | FLAMA=%s | DHT=OK\n",
        lecturas.temperaturaC,
        lecturas.humedadPct,
        lecturas.gasAdc,
        lecturas.gasAdcCrudo,
        lecturas.flamaDetectada ? "FUEGO" : "NO");
  } else {
    Serial.printf(
        "[SENSOR] T=-- | H=-- | GAS=%d (crudo=%d) | FLAMA=%s | DHT=ERROR\n",
        lecturas.gasAdc,
        lecturas.gasAdcCrudo,
        lecturas.flamaDetectada ? "FUEGO" : "NO");
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println(F("=============================================="));
  Serial.println(F(" IndoorWatch - Rover IoT de monitoreo"));
  Serial.println(F(" Etapa 1 + motores L298N + mando Bluetooth"));
  Serial.println(F("=============================================="));

  sensoresInit();
  motoresInit();
  controlBtInit();
  wifiInit();
  telegramInit();
  alertasInit();

  Serial.println(F("[SISTEMA] Listo. Monitor Serial a 115200 baudios."));
}

void loop() {
  // Mando Bluetooth y motores primero: el control no espera a la red.
  controlBtTick();
  motoresTick();
  procesarComandoSerial();

  // Wi-Fi: mantiene la reconexión aunque no haya datos nuevos.
  wifiTick();

  // Adquisición no bloqueante cada INTERVALO_SENSORES_MS.
  if (sensoresTick()) {
    imprimirLecturas(sensoresLecturas());
  }

  const LecturasAmbientales& lecturas = sensoresLecturas();

  // Seguridad primero: las alertas no esperan al polling de Telegram.
  alertasTick(lecturas);

  // Comandos bajo demanda y aviso de conexión.
  telegramTick(lecturas);

  // Cede tiempo al Bluetooth/Wi-Fi y evita el watchdog.
  delay(1);
}
