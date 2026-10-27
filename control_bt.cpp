#include "control_bt.h"
#include "config.h"
#include "motores.h"

// Permite compilar con el core estándar de ESP32 (sin Bluetooth) y con el
// core ESP32 + Bluepad32. El control por mando solo existe en el segundo.
#if defined(__has_include)
#if __has_include(<Bluepad32.h>)
#define CONTROL_BT_DISPONIBLE 1
#else
#define CONTROL_BT_DISPONIBLE 0
#endif
#else
#define CONTROL_BT_DISPONIBLE 0
#endif

#if CONTROL_BT_DISPONIBLE

#include <Bluepad32.h>

namespace {

ControllerPtr mandos[BP32_MAX_GAMEPADS];
ControllerPtr mandoActivo = nullptr;

bool mandoConectado = false;
String nombreMando;
unsigned long ultimaActividadMandoMs = 0;
bool timeoutAplicado = false;

bool mandoSigueConectado(ControllerPtr ctl) {
  return ctl != nullptr && ctl->isConnected();
}

void onConnectedController(ControllerPtr ctl) {
  for (uint8_t i = 0; i < BP32_MAX_GAMEPADS; ++i) {
    if (mandos[i] == nullptr) {
      mandos[i] = ctl;
      mandoActivo = ctl;
      mandoConectado = true;
      nombreMando = ctl->getModelName();
      ultimaActividadMandoMs = millis();
      timeoutAplicado = false;

      Serial.printf("[BT] Mando conectado: %s (indice %u)\n",
                    nombreMando.c_str(), static_cast<unsigned>(i));
      ctl->setPlayerLEDs(1);
      return;
    }
  }

  Serial.println(F("[BT] Mando conectado, pero no hay ranura libre."));
}

void onDisconnectedController(ControllerPtr ctl) {
  for (uint8_t i = 0; i < BP32_MAX_GAMEPADS; ++i) {
    if (mandos[i] == ctl) {
      Serial.printf("[BT] Mando desconectado (indice %u).\n",
                    static_cast<unsigned>(i));
      mandos[i] = nullptr;
      break;
    }
  }

  mandoActivo = nullptr;
  mandoConectado = false;
  nombreMando = "";
  timeoutAplicado = false;

  // Si se cae el enlace, el rover no debe seguir con la ultima orden.
  motoresDetener();
}

DireccionMovimiento leerDireccion(ControllerPtr ctl) {
  int32_t ejeY = ctl->axisY();
  int32_t ejeX = ctl->axisRX();

  if (MANDO_EJE_Y_INVERTIDO) {
    ejeY = -ejeY;
  }
  if (MANDO_EJE_X_INVERTIDO) {
    ejeX = -ejeX;
  }

  const int32_t magnitudY = abs(static_cast<int>(ejeY));
  const int32_t magnitudX = abs(static_cast<int>(ejeX));

  if (magnitudY < MANDO_ZONA_MUERTA && magnitudX < MANDO_ZONA_MUERTA) {
    return DireccionMovimiento::DETENIDO;
  }

  // Se prioriza el eje con mayor desplazamiento. Asi el rover no intenta
  // avanzar y girar al mismo tiempo con el control de direccion actual.
  if (magnitudY >= magnitudX) {
    return ejeY < 0 ? DireccionMovimiento::ADELANTE
                    : DireccionMovimiento::ATRAS;
  }

  return ejeX > 0 ? DireccionMovimiento::DERECHA
                  : DireccionMovimiento::IZQUIERDA;
}

void procesarMando(ControllerPtr ctl) {
  ultimaActividadMandoMs = millis();
  timeoutAplicado = false;

  // Boton A: paro inmediato. Los demas botones quedan libres para
  // funciones futuras (demo, bocina, luces, etc.).
  if (ctl->a()) {
    motoresDetener();
    return;
  }

  motoresControlContinuo(leerDireccion(ctl));
}

}  // namespace

void controlBtInit() {
  Serial.printf("[BT] %s\n", BP32.firmwareVersion());

  const uint8_t* direccion = BP32.localBdAddress();
  Serial.printf("[BT] Direccion local: %02X:%02X:%02X:%02X:%02X:%02X\n",
                direccion[0], direccion[1], direccion[2], direccion[3],
                direccion[4], direccion[5]);

  BP32.setup(&onConnectedController, &onDisconnectedController);
  BP32.enableVirtualDevice(false);
  BP32.enableNewBluetoothConnections(true);

  Serial.println(F("[BT] Esperando mando GameSir Nova Lite..."));
  Serial.println(F("[BT] Pon el mando en modo Android (Home + A) o Switch."));
}

void controlBtTick() {
  const bool datosActualizados = BP32.update();

  if (datosActualizados) {
    for (uint8_t i = 0; i < BP32_MAX_GAMEPADS; ++i) {
      ControllerPtr ctl = mandos[i];
      if (ctl != nullptr && ctl->isConnected() && ctl->hasData()) {
        mandoActivo = ctl;
        procesarMando(ctl);
        break;
      }
    }
  }

  if (mandoConectado && !timeoutAplicado &&
      (millis() - ultimaActividadMandoMs > MANDO_TIMEOUT_MS)) {
    motoresDetener();
    timeoutAplicado = true;
    Serial.println(F("[BT] Sin datos del mando; motores detenidos."));
  }

  if (mandoActivo != nullptr && !mandoSigueConectado(mandoActivo)) {
    mandoActivo = nullptr;
  }
}

bool controlBtConectado() {
  return mandoConectado;
}

const char* controlBtNombre() {
  return nombreMando.length() > 0 ? nombreMando.c_str() : "desconectado";
}

#else

void controlBtInit() {
  Serial.println(
      F("[BT] Bluepad32 no instalado. Compila con la placa "
        "\"ESP32 + Bluepad32\" para usar el mando."));
}

void controlBtTick() {}

bool controlBtConectado() {
  return false;
}

const char* controlBtNombre() {
  return "Bluepad32 no instalado";
}

#endif
