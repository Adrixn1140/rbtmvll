#include "wifi_manager.h"
#include "config.h"
#include "secrets.h"

#include <WiFi.h>

namespace {
bool habilitado = false;
bool conectadoPrevio = false;
unsigned long ultimoIntentoMs = 0;
}  // namespace

void wifiInit() {
  wifiEncender();
}

void wifiEncender() {
  if (habilitado) {
    return;
  }

  habilitado = true;
  conectadoPrevio = false;

  WiFi.mode(WIFI_STA);
  WiFi.setHostname("IndoorWatch-Rover");
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  Serial.print(F("[WIFI] Encendiendo. Conectando a la red: "));
  Serial.println(WIFI_SSID);

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  ultimoIntentoMs = millis();
}

void wifiApagar() {
  if (!habilitado) {
    return;
  }

  habilitado = false;
  conectadoPrevio = false;

  WiFi.disconnect(true, false);
  WiFi.mode(WIFI_OFF);

  Serial.println(
      F("[WIFI] Apagado. La radio queda libre para el mando Bluetooth."));
}

void wifiTick() {
  if (!habilitado) {
    return;
  }

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

bool wifiHabilitado() {
  return habilitado;
}

bool wifiConectado() {
  return habilitado && WiFi.status() == WL_CONNECTED;
}

String wifiIp() {
  if (!habilitado) {
    return F("apagado");
  }
  return WiFi.localIP().toString();
}

int wifiRssi() {
  if (!habilitado) {
    return 0;
  }
  return static_cast<int>(WiFi.RSSI());
}
