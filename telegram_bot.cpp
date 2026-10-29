#include "telegram_bot.h"
#include "config.h"
#include "secrets.h"
#include "wifi_manager.h"
#include "motores.h"
#include "control_bt.h"
#include "bluetooth_serial.h"

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

  msg += F("🚗 Motores: ");
  msg += motoresEstadoTexto();
  msg += F("\n🎮 Mando: ");
  msg += controlBtConectado() ? controlBtNombre() : "desconectado";
  msg += F("\n📡 Bluetooth serial: ");
  msg += bluetoothSerialEstado();
  msg += F("\n");

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
  msg += F("/motores — Control de motores\n");
  msg += F("/demo — Prueba de motores\n");
  msg += F("/mando — Estado del mando GameSir\n");
  msg += F("/bluetooth — Respaldo por Bluetooth serial\n");
  msg += F("/id — ID de este chat\n");
  msg += F("/ayuda — Esta ayuda");
  telegramEnviarMensaje(msg);
}

void enviarAyudaMotores() {
  String msg = F("🚗 *Control de motores — L298N*\n\n");
  msg += F("/demo — Secuencia automática de prueba\n");
  msg += F("/adelante — Avanzar 1.5 s\n");
  msg += F("/atras — Retroceder 1.5 s\n");
  msg += F("/izquierda — Girar a la izquierda 1.5 s\n");
  msg += F("/derecha — Girar a la derecha 1.5 s\n");
  msg += F("/detener — Parar inmediatamente\n\n");
  msg += F("Los movimientos manuales duran ");
  msg += String(DURACION_MOVIMIENTO_MANUAL_MS / 1000.0f, 1);
  msg += F(" s y luego se detienen solos.\n\n");
  msg += F("🎮 Mando: stick izquierdo arriba/abajo, stick derecho ");
  msg += F("izquierda/derecha; botón A = paro.\n");
  msg += F("📡 Sin mando, por USB/Bluetooth serial: a/r/i/d/s/p/m/h.\n");
  msg += F("a/r/i/d siguen hasta s o hasta ");
  msg += String(DURACION_CONTROL_CONTINUO_MS / 1000UL);
  msg += F(" s sin una nueva orden.");
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
  } else if (comando == "/mando") {
    if (controlBtConectado()) {
      telegramEnviarMensaje(String(F("🎮 Mando conectado: ")) +
                            controlBtNombre());
    } else {
      telegramEnviarMensaje(
          F("🎮 Mando desconectado. Enciéndelo en modo Android (Home + A) "
            "o Switch (Home + Y) y espera el parpadeo rápido."));
    }
  } else if (comando == "/bluetooth") {
    String info = F("📡 *Bluetooth serial*\n\n");
    info += F("Módulo: HC-05 / HC-06 / HM-10 (UART).\n");
    info += F("Cableado: TX del módulo → GPIO16, RX → GPIO17, GND común.\n");
    info += F("Baudios: 9600.\n\n");
    info += F("1. Empareja el celular con el módulo (PIN 1234 o 0000).\n");
    info += F("2. Abre una app de terminal Bluetooth.\n");
    info += F("3. Envía una sola letra: a/r/i/d/s/p.\n\n");
    info += F("a=adelante, r=atrás, i=izquierda, d=derecha, ");
    info += F("s=detener, p=demo, m=estado, h=ayuda.");
    telegramEnviarMensaje(info);
  } else if (comando == "/motores" || comando == "/control") {
    enviarAyudaMotores();
  } else if (comando == "/demo" || comando == "/prueba") {
    telegramEnviarMensaje(
        F("🚗 Demo iniciada: adelante, atrás, izquierda y derecha."));
    motoresDemo();
  } else if (comando == "/adelante") {
    telegramEnviarMensaje(F("⬆️ Adelante durante 1.5 s."));
    motoresMover(DireccionMovimiento::ADELANTE,
                 DURACION_MOVIMIENTO_MANUAL_MS);
  } else if (comando == "/atras") {
    telegramEnviarMensaje(F("⬇️ Atrás durante 1.5 s."));
    motoresMover(DireccionMovimiento::ATRAS,
                 DURACION_MOVIMIENTO_MANUAL_MS);
  } else if (comando == "/izquierda") {
    telegramEnviarMensaje(F("⬅️ Izquierda durante 1.5 s."));
    motoresMover(DireccionMovimiento::IZQUIERDA,
                 DURACION_MOVIMIENTO_MANUAL_MS);
  } else if (comando == "/derecha") {
    telegramEnviarMensaje(F("➡️ Derecha durante 1.5 s."));
    motoresMover(DireccionMovimiento::DERECHA,
                 DURACION_MOVIMIENTO_MANUAL_MS);
  } else if (comando == "/detener" || comando == "/stop" ||
             comando == "/frenar") {
    motoresDetener();
    telegramEnviarMensaje(F("🛑 Motores detenidos."));
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
