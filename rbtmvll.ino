#include <Arduino.h>

#include "config.h"
#include "sensores.h"
#include "wifi_manager.h"
#include "telegram_bot.h"
#include "alertas.h"
#include "motores.h"
#include "comandos.h"
#include "bluetooth_serial.h"

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
  Serial.println(F(" Etapa 1 + motores L298N + Bluetooth serial"));
  Serial.println(F("=============================================="));

  sensoresInit();
  motoresInit();
  bluetoothSerialInit();
  wifiInit();
  telegramInit();
  alertasInit();

  Serial.println(
      F("[SISTEMA] Listo. USB 115200 | Bluetooth serial 9600 baudios."));
}

void loop() {
  // Bluetooth serial y motores primero: el control no espera a la red.
  bluetoothSerialTick();
  motoresTick();
  comandosProcesar(Serial, "USB");

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

  // Cede tiempo al Wi-Fi y evita el watchdog.
  delay(1);
}
