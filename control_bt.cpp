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
bool mandoControlaMovimiento = false;
bool debugMando = false;
unsigned long ultimoDebugMandoMs = 0;

// Bluepad32 4.1.0 solo ejecuta un comando "safe" por loop. Para olvidar
// claves y volver a buscar el mando hay que hacerlo en dos pasos separados.
bool reactivarBusquedaPendiente = false;
unsigned long reactivarBusquedaMs = 0;

bool mandoSigueConectado(ControllerPtr ctl) {
  return ctl != nullptr && ctl->isConnected();
}

ControllerPtr primerMandoConectado() {
  for (uint8_t i = 0; i < BP32_MAX_GAMEPADS; ++i) {
    if (mandos[i] != nullptr && mandos[i]->isConnected()) {
      return mandos[i];
    }
  }
  return nullptr;
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
      mandoControlaMovimiento = false;

      const ControllerProperties propiedades = ctl->getProperties();
      Serial.printf("[BT] Mando conectado: %s (indice %u)\n",
                    nombreMando.c_str(), static_cast<unsigned>(i));
      Serial.printf(
          "[BT] VID=0x%04X PID=0x%04X tipo=%u flags=0x%04X "
          "addr=%02X:%02X:%02X:%02X:%02X:%02X\n",
          static_cast<unsigned>(propiedades.vendor_id),
          static_cast<unsigned>(propiedades.product_id),
          static_cast<unsigned>(propiedades.type),
          static_cast<unsigned>(propiedades.flags),
          propiedades.btaddr[0], propiedades.btaddr[1], propiedades.btaddr[2],
          propiedades.btaddr[3], propiedades.btaddr[4], propiedades.btaddr[5]);
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

  mandoActivo = primerMandoConectado();
  mandoConectado = (mandoActivo != nullptr);
  nombreMando = mandoConectado ? mandoActivo->getModelName() : "";
  timeoutAplicado = false;
  mandoControlaMovimiento = false;

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
    mandoControlaMovimiento = false;
    return;
  }

  const DireccionMovimiento direccion = leerDireccion(ctl);
  if (direccion == DireccionMovimiento::DETENIDO) {
    // Solo detiene si el mando era quien estaba moviendo el rover. Así un
    // mando conectado pero en reposo no cancela órdenes de Telegram/USB.
    if (mandoControlaMovimiento) {
      motoresDetener();
      mandoControlaMovimiento = false;
    }
    return;
  }

  motoresControlContinuo(direccion);
  mandoControlaMovimiento = true;
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
  Serial.println(
      F("[BT] Apagado, manten A+Home (Android, amarillo) o Y+Home "
        "(Switch, rojo) hasta parpadeo rapido."));
  Serial.println(
      F("[BT] Serial: m=estado b=olvidar claves e=rebuscar x=debug"));
}

void controlBtTick() {
  const bool datosActualizados = BP32.update();

  if (reactivarBusquedaPendiente &&
      static_cast<long>(millis() - reactivarBusquedaMs) >= 0) {
    BP32.enableNewBluetoothConnections(true);
    reactivarBusquedaPendiente = false;
    Serial.println(F("[BT] Busqueda Bluetooth reactivada."));
  }

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

  if (mandoConectado && mandoControlaMovimiento && !timeoutAplicado &&
      (millis() - ultimaActividadMandoMs > MANDO_TIMEOUT_MS)) {
    motoresDetener();
    mandoControlaMovimiento = false;
    timeoutAplicado = true;
    Serial.println(F("[BT] Sin datos del mando; motores detenidos."));
  }

  if (mandoActivo != nullptr && !mandoSigueConectado(mandoActivo)) {
    mandoActivo = nullptr;
  }

  if (debugMando && (millis() - ultimoDebugMandoMs >= 250UL)) {
    ultimoDebugMandoMs = millis();
    ControllerPtr ctl = primerMandoConectado();
    if (ctl != nullptr) {
      Serial.printf(
          "[BT][DBG] L=(%d,%d) R=(%d,%d) botones=0x%04X dpad=0x%02X dir=%s\n",
          static_cast<int>(ctl->axisX()), static_cast<int>(ctl->axisY()),
          static_cast<int>(ctl->axisRX()), static_cast<int>(ctl->axisRY()),
          static_cast<unsigned>(ctl->buttons()),
          static_cast<unsigned>(ctl->dpad()), motoresEstadoTexto());
    } else {
      Serial.println(F("[BT][DBG] Sin mando conectado."));
    }
  }
}

void controlBtImprimirEstado() {
  ControllerPtr ctl = primerMandoConectado();
  if (ctl == nullptr) {
    Serial.println(F("[BT] Sin mando conectado."));
    return;
  }

  const unsigned long edadDatos = millis() - ultimaActividadMandoMs;
  Serial.printf("[BT] Mando: %s | ultimo dato hace %lu ms | dir=%s\n",
                ctl->getModelName().c_str(), edadDatos, motoresEstadoTexto());
}

void controlBtOlvidarClaves() {
  BP32.forgetBluetoothKeys();
  reactivarBusquedaPendiente = true;
  reactivarBusquedaMs = millis() + 500UL;
  timeoutAplicado = false;
  Serial.println(
      F("[BT] Claves Bluetooth olvidadas. Reinicia el mando y ponlo en "
        "modo emparejamiento."));
}

void controlBtReiniciarBusqueda() {
  BP32.enableNewBluetoothConnections(false);
  reactivarBusquedaPendiente = true;
  reactivarBusquedaMs = millis() + 300UL;
  Serial.println(F("[BT] Reiniciando la busqueda Bluetooth..."));
}

void controlBtAlternarDebug() {
  debugMando = !debugMando;
  ultimoDebugMandoMs = millis();
  Serial.printf("[BT] Debug de ejes %s.\n",
                debugMando ? "activado" : "desactivado");
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

void controlBtImprimirEstado() {
  Serial.println(F("[BT] Bluepad32 no instalado."));
}

void controlBtOlvidarClaves() {
  Serial.println(F("[BT] Bluepad32 no instalado."));
}

void controlBtReiniciarBusqueda() {
  Serial.println(F("[BT] Bluepad32 no instalado."));
}

void controlBtAlternarDebug() {
  Serial.println(F("[BT] Bluepad32 no instalado."));
}

bool controlBtConectado() {
  return false;
}

const char* controlBtNombre() {
  return "Bluepad32 no instalado";
}

#endif
