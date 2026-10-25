#include "wifi_manager.h"
#include "config.h"
#include "secrets.h"

#include <WiFi.h>

namespace {
bool conectadoPrevio = false;
unsigned long ultimoIntentoMs = 0;
}  // namespace

void wifiInit() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("IndoorWatch-Rover");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.print(F("[WIFI] Conectando a la red: "));
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  ultimoIntentoMs = millis();
}

void wifiTick() {
  const unsigned long ahora = millis();
  const bool conectado = wifiConectado();

  if (conectado && !conectadoPrevio) {
    Serial.print(F("[WIFI] Conectado. IP: "));
    Serial.print(WiFi.localIP());
    Serial.print(F(" | RSSI: "));
    Serial.print(WiFi.RSSI());
    Serial.println(F(" dBm"));
  } else if (!conectado && conectadoPrevio) {
    Serial.println(F("[WIFI] Conexion perdida. Se reintentara automaticamente."));
  }

  // Reconexión no bloqueante: el loop sigue atendiendo sensores y Telegram.
  if (!conectado && (ahora - ultimoIntentoMs >= INTERVALO_RECONEXION_WIFI_MS)) {
    ultimoIntentoMs = ahora;
    Serial.println(F("[WIFI] Reintentando conexion..."));
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }

  conectadoPrevio = conectado;
}

bool wifiConectado() {
  return WiFi.status() == WL_CONNECTED;
}

String wifiIp() {
  return WiFi.localIP().toString();
}

int wifiRssi() {
  return static_cast<int>(WiFi.RSSI());
}
