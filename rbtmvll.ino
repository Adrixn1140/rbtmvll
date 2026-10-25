#include <Arduino.h>

#include "config.h"
#include "sensores.h"
#include "wifi_manager.h"
#include "telegram_bot.h"
#include "alertas.h"

namespace {

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
  Serial.println(F(" Etapa 1: sensores ambientales + Telegram"));
  Serial.println(F("=============================================="));

  sensoresInit();
  wifiInit();
  telegramInit();
  alertasInit();

  Serial.println(F("[SISTEMA] Listo. Monitor Serial a 115200 baudios."));
}

void loop() {
  // Wi-Fi primero: mantiene la reconexión aunque no haya datos nuevos.
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
}
