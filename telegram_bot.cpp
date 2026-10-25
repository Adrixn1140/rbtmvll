#include "telegram_bot.h"
#include "config.h"
#include "secrets.h"
#include "wifi_manager.h"

#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

namespace {

WiFiClientSecure clienteSeguro;
UniversalTelegramBot bot(BOT_TOKEN, clienteSeguro);

unsigned long ultimaConsultaMs = 0;
unsigned long ultimoIntentoAvisoMs = 0;
bool wifiDisponiblePrevio = false;
bool avisoPendiente = false;
bool avisoIntentado = false;

String formatearUptime() {
  const unsigned long totalSegundos = millis() / 1000UL;
  char buffer[24];
  snprintf(buffer, sizeof(buffer), "%02lu:%02lu:%02lu",
           totalSegundos / 3600UL,
           (totalSegundos / 60UL) % 60UL,
           totalSegundos % 60UL);
  return String(buffer);
}

String normalizarComando(String texto) {
  texto.trim();
  const int arroba = texto.indexOf('@');
  if (arroba > 0) {
    texto = texto.substring(0, arroba);
  }
  texto.toLowerCase();
  return texto;
}

String reporteEstado(const LecturasAmbientales& lecturas) {
  String msg = F("📊 *IndoorWatch · Estado ambiental*\n\n");

  if (lecturas.ultimaActualizacionMs == 0) {
    msg += F("⏳ Esperando la primera lectura de los sensores...\n\n");
  }

  if (lecturas.dhtValida) {
    msg += F("🌡️ Temperatura: ");
    msg += String(lecturas.temperaturaC, 1);
    msg += F(" °C\n💧 Humedad: ");
    msg += String(lecturas.humedadPct, 1);
    msg += F(" %\n");
  } else {
    msg += F("🌡️ Temperatura: sin lectura válida\n");
    msg += F("💧 Humedad: sin lectura válida\n");
  }

  msg += F("💨 Gas/humo: ");
  msg += String(lecturas.gasAdc);
  msg += F(" ADC ");
  msg += (lecturas.gasAdc > UMBRAL_GAS_ADC) ? F("⚠️ (elevado)\n")
                                             : F("✅ (normal)\n");

  msg += F("🔥 Flama IR: ");
  msg += lecturas.flamaDetectada ? F("🚨 FUEGO DETECTADO\n")
                                 : F("✅ sin detección\n");

  msg += F("⏱️ Última lectura: ");
  if (lecturas.ultimaActualizacionMs == 0) {
    msg += F("pendiente\n");
  } else {
    const unsigned long segundos =
        (millis() - lecturas.ultimaActualizacionMs) / 1000UL;
    msg += F("hace ");
    msg += String(segundos);
    msg += F(" s\n");
  }

  msg += F("📶 Wi-Fi: ");
  msg += String(wifiRssi());
  msg += F(" dBm\n🕒 Uptime: ");
  msg += formatearUptime();
  return msg;
}

void enviarAyuda() {
  String msg = F("👋 *IndoorWatch — Rover IoT*\n\n");
  msg += F("Monitoreo ambiental en interiores con ESP32.\n\n");
  msg += F("*Comandos disponibles:*\n");
  msg += F("/estado — Lecturas actuales\n");
  msg += F("/sensores — Igual que /estado\n");
  msg += F("/umbrales — Límites de alerta\n");
  msg += F("/ping — Estado del sistema\n");
  msg += F("/id — ID de este chat\n");
  msg += F("/ayuda — Esta ayuda");
  telegramEnviarMensaje(msg);
}

void procesarMensaje(const telegramMessage& mensaje,
                     const LecturasAmbientales& lecturas) {
  const String chatId = mensaje.chat_id;

  // Seguridad: solo el chat configurado puede consultar o dar comandos.
  if (chatId != String(CHAT_ID)) {
    Serial.print(F("[TELEGRAM] Mensaje ignorado de chat no autorizado: "));
    Serial.println(chatId);
    return;
  }

  const String comando = normalizarComando(mensaje.text);
  if (comando.length() == 0) {
    return;
  }

  Serial.print(F("[TELEGRAM] Comando recibido: "));
  Serial.println(comando);

  if (comando == "/start" || comando == "/ayuda" || comando == "/help") {
    enviarAyuda();
  } else if (comando == "/estado" || comando == "/sensores") {
    telegramEnviarMensaje(reporteEstado(lecturas));
  } else if (comando == "/umbrales") {
    String info = F("⚙️ *Umbrales de alerta*\n\n");
    info += F("🌡️ Temperatura: > ");
    info += String(UMBRAL_TEMP_C, 1);
    info += F(" °C\n💨 Gas/humo: > ");
    info += String(UMBRAL_GAS_ADC);
    info += F(" ADC\n🔥 Flama: activa en LOW\n⏳ Cooldown de alertas: ");
    info += String(COOLDOWN_ALERTA_MS / 1000UL);
    info += F(" s\n📊 Lectura de sensores: cada ");
    info += String(INTERVALO_SENSORES_MS / 1000.0f, 1);
    info += F(" s");
    telegramEnviarMensaje(info);
  } else if (comando == "/ping") {
    String pong = F("🏓 *Pong*\n\n🕒 Uptime: ");
    pong += formatearUptime();
    pong += F("\n📶 Wi-Fi: ");
    pong += String(wifiRssi());
    pong += F(" dBm\n🌐 IP: ");
    pong += wifiIp();
    pong += F("\n🧠 Heap libre: ");
    pong += String(ESP.getFreeHeap());
    pong += F(" bytes");
    telegramEnviarMensaje(pong);
  } else if (comando == "/id") {
    telegramEnviarMensaje(String(F("🆔 Chat ID autorizado: ")) + chatId);
  } else if (comando.startsWith("/")) {
    telegramEnviarMensaje(
        F("❓ Comando no reconocido. Usa /ayuda para ver las opciones."));
  }
}

}  // namespace

void telegramInit() {
  // Prototipo: no se valida la cadena de certificados de Telegram.
  // Para producción, fijar el certificado raíz correspondiente.
  clienteSeguro.setInsecure();
  bot.longPoll = 0;
  ultimaConsultaMs = millis();
  Serial.println(F("[TELEGRAM] Cliente TLS listo (modo prototipo)."));
}

bool telegramEnviarMensaje(const String& texto) {
  if (!wifiConectado()) {
    return false;
  }

  const bool ok = bot.sendMessage(String(CHAT_ID), texto, "Markdown");
  if (!ok) {
    Serial.println(F("[TELEGRAM] Fallo al enviar el mensaje."));
  }
  return ok;
}

void telegramTick(const LecturasAmbientales& lecturas) {
  if (!wifiConectado()) {
    wifiDisponiblePrevio = false;
    return;
  }

  const unsigned long ahora = millis();

  if (!wifiDisponiblePrevio) {
    wifiDisponiblePrevio = true;
    avisoPendiente = true;
    avisoIntentado = false;
  }

  // Aviso de conexión (una vez por reconexión; reintenta si falla el envío).
  if (avisoPendiente) {
    const bool tocaIntentar =
        !avisoIntentado ||
        (ahora - ultimoIntentoAvisoMs >= INTERVALO_REINTENTO_ENVIO_MS);

    if (tocaIntentar) {
      avisoIntentado = true;
      ultimoIntentoAvisoMs = ahora;

      String aviso = F("🟢 *IndoorWatch en línea*\n\n");
      aviso += F("Sistema de monitoreo ambiental iniciado.\n");
      aviso += F("Usa /estado para consultar las lecturas.");

      if (telegramEnviarMensaje(aviso)) {
        avisoPendiente = false;
        Serial.println(F("[TELEGRAM] Aviso de inicio enviado correctamente."));
      }
    }
  }

  // Polling no bloqueante de comandos (un mensaje por consulta).
  if (ahora - ultimaConsultaMs < INTERVALO_TELEGRAM_MS) {
    return;
  }
  ultimaConsultaMs = ahora;

  const int numMensajes = bot.getUpdates(bot.last_message_received + 1);
  if (numMensajes <= 0) {
    return;
  }

  for (int i = 0; i < numMensajes; ++i) {
    procesarMensaje(bot.messages[i], lecturas);
  }
}
