#include "bluetooth_serial.h"

#include "comandos.h"
#include "config.h"

namespace {

bool bluetoothInicializado = false;

}  // namespace

void bluetoothSerialInit() {
  // HC-05/HC-06/HM-10: UART a 9600 baudios por defecto.
  // El pin RX del módulo se conecta al TX2 de la ESP32 (GPIO17) y el TX del
  // módulo al RX2 (GPIO16). GND siempre en común.
  Serial2.begin(BT_BAUDIOS, SERIAL_8N1, PIN_BT_RX, PIN_BT_TX);
  bluetoothInicializado = true;

  Serial.printf(
      "[BT] Modulo serial listo: RX=GPIO%d TX=GPIO%d @ %lu baudios.\n",
      PIN_BT_RX, PIN_BT_TX, BT_BAUDIOS);
  Serial.println(F("[BT] Empareja el celular y envia a/r/i/d/s/p."));
  Serial.println(F("[BT] Ayuda: h | estado: m"));
}

void bluetoothSerialTick() {
  if (!bluetoothInicializado) {
    return;
  }

  // Se llama siempre para que el parser pueda cerrar una letra enviada sin
  // Enter después de un breve silencio.
  comandosProcesar(Serial2, "BT", true);
}

void bluetoothSerialDescartar() {
  // Mientras el mando tiene el control, se tira lo que llegue por el HC-05
  // para que no se ejecute una orden vieja al desconectar el mando.
  comandosReset(Serial2);
  while (Serial2.available() > 0) {
    Serial2.read();
  }
}

const char* bluetoothSerialEstado() {
  static char estado[80];
  snprintf(estado, sizeof(estado),
           "modulo serial listo (RX=GPIO%u, TX=GPIO%u, %lu baudios)",
           PIN_BT_RX, PIN_BT_TX, BT_BAUDIOS);
  return estado;
}
