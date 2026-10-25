#include "alertas.h"
#include "config.h"
#include "telegram_bot.h"

namespace {

constexpr uint8_t ALERTA_TEMP  = 0x01;
constexpr uint8_t ALERTA_GAS   = 0x02;
constexpr uint8_t ALERTA_FLAMA = 0x04;

uint8_t mascaraNotificada = 0;
bool condicionTempActiva = false;
unsigned long ultimoEnvioMs = 0;
unsigned long ultimoIntentoMs = 0;
bool intentoFallido = false;

uint8_t evaluarAlertas(const LecturasAmbientales& lecturas) {
  uint8_t mascara = 0;

  // Si el DHT22 falla, se conserva la última condición conocida para no
  // declarar una normalización sin datos confiables.
  if (lecturas.dhtValida) {
    condicionTempActiva = (lecturas.temperaturaC > UMBRAL_TEMP_C);
  }
  if (condicionTempActiva) {
    mascara |= ALERTA_TEMP;
  }
  if (lecturas.gasAdc > UMBRAL_GAS_ADC) {
    mascara |= ALERTA_GAS;
  }
  if (lecturas.flamaDetectada) {
    mascara |= ALERTA_FLAMA;
  }

  return mascara;
}

String construirAlerta(uint8_t mascara, const LecturasAmbientales& lecturas) {
  String msg = F("🚨 *ALERTA AMBIENTAL — IndoorWatch*\n\n");

  if (mascara & ALERTA_TEMP) {
    msg += F("🌡️ *Temperatura crítica:* ");
    msg += String(lecturas.temperaturaC, 1);
    msg += F(" °C (límite ");
    msg += String(UMBRAL_TEMP_C, 1);
    msg += F(" °C)\n");
    if (!lecturas.dhtValida) {
      msg += F("⚠️ DHT22 sin lectura actual; se muestra la última válida.\n");
    }
  }

  if (mascara & ALERTA_GAS) {
    msg += F("💨 *Gas/humo elevado:* ");
    msg += String(lecturas.gasAdc);
    msg += F(" ADC (límite ");
    msg += String(UMBRAL_GAS_ADC);
    msg += F(" ADC)\n");
  }

  if (mascara & ALERTA_FLAMA) {
    msg += F("🔥 *Flama detectada:* sensor IR en LOW\n");
  }

  msg += F("\nAlerta automática. Se repetirá cada ");
  msg += String(COOLDOWN_ALERTA_MS / 1000UL);
  msg += F(" s mientras la condición persista.");
  return msg;
}

String construirNormalizacion(uint8_t mascara,
                              const LecturasAmbientales& lecturas) {
  String msg = F("✅ *Valores normalizados — IndoorWatch*\n\n");

  if (mascara & ALERTA_TEMP) {
    msg += F("🌡️ Temperatura: ");
    if (lecturas.dhtValida) {
      msg += String(lecturas.temperaturaC, 1);
      msg += F(" °C\n");
    } else {
      msg += F("sin lectura válida\n");
    }
  }

  if (mascara & ALERTA_GAS) {
    msg += F("💨 Gas/humo: ");
    msg += String(lecturas.gasAdc);
    msg += F(" ADC\n");
  }

  if (mascara & ALERTA_FLAMA) {
    msg += F("🔥 Flama: sin detección\n");
  }

  msg += F("\nEl sistema continúa monitoreando.");
  return msg;
}

}  // namespace

void alertasInit() {
  mascaraNotificada = 0;
  condicionTempActiva = false;
  ultimoEnvioMs = 0;
  ultimoIntentoMs = 0;
  intentoFallido = false;

  Serial.printf("[ALERTAS] Umbrales: temp > %.1f C, gas > %d, flama LOW.\n",
                UMBRAL_TEMP_C, UMBRAL_GAS_ADC);
}

void alertasTick(const LecturasAmbientales& lecturas) {
  const uint8_t mascara = evaluarAlertas(lecturas);
  const unsigned long ahora = millis();

  // Tras un fallo de envío se reintenta cada 10 s, sin bloquear el loop.
  const bool puedeIntentar =
      !intentoFallido ||
      (ahora - ultimoIntentoMs >= INTERVALO_REINTENTO_ENVIO_MS);

  // Transición de alerta activa a estado normal.
  if (mascara == 0 && mascaraNotificada != 0) {
    if (!puedeIntentar) {
      return;
    }
    ultimoIntentoMs = ahora;

    if (telegramEnviarMensaje(construirNormalizacion(mascaraNotificada, lecturas))) {
      Serial.println(F("[ALERTAS] Normalizacion notificada."));
      mascaraNotificada = 0;
      ultimoEnvioMs = ahora;
      intentoFallido = false;
    } else {
      intentoFallido = true;
    }
    return;
  }

  if (mascara == 0) {
    return;
  }

  const bool cambio = (mascara != mascaraNotificada);
  const bool cooldownVencido =
      (ultimoEnvioMs == 0) ||
      (ahora - ultimoEnvioMs >= COOLDOWN_ALERTA_MS);

  if ((!cambio && !cooldownVencido) || !puedeIntentar) {
    return;
  }

  ultimoIntentoMs = ahora;

  if (telegramEnviarMensaje(construirAlerta(mascara, lecturas))) {
    Serial.printf("[ALERTAS] Alerta enviada (mascara=0x%02X).\n", mascara);
    mascaraNotificada = mascara;
    ultimoEnvioMs = ahora;
    intentoFallido = false;
  } else {
    Serial.println(F("[ALERTAS] No se pudo enviar; se reintentara."));
    intentoFallido = true;
  }
}
