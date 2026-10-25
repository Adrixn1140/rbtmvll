#include <Arduino.h>

#include "config.h"
#include "sensores.h"

// Versión incremental de la Etapa 1: adquisición no bloqueante de sensores
// y validación por Monitor Serial. La integración con Telegram se agrega en
// el siguiente commit.

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
  Serial.println(F(" IndoorWatch - Etapa 1: sensores ambientales"));
  Serial.println(F("=============================================="));

  sensoresInit();

  Serial.println(F("[SISTEMA] Listo. Monitor Serial a 115200 baudios."));
}

void loop() {
  if (sensoresTick()) {
    imprimirLecturas(sensoresLecturas());
  }
}
