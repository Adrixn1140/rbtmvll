#include "oled.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <math.h>

#include "config.h"
#include "control_bt.h"
#include "modo.h"
#include "servo_ultrasonido.h"
#include "wifi_manager.h"

namespace {

Adafruit_SSD1306 display(OLED_ANCHO, OLED_ALTO, &Wire, -1);

bool disponible = false;
bool forzarRefresco = true;
unsigned long ultimoRefrescoMs = 0;
unsigned long mostrarDatosHastaMs = 0;

String textoDistancia(float distancia) {
  if (isnan(distancia) || distancia <= 0.0f) {
    return F("--");
  }
  return String(distancia, 0);
}

void imprimirLinea(uint8_t linea, const String& texto) {
  display.setCursor(0, linea * 10);
  display.print(texto);
}

void dibujarManual(const LecturasAmbientales& lecturas) {
  const bool mostrandoDatos =
      static_cast<long>(millis() - mostrarDatosHastaMs) < 0;

  if (mostrandoDatos) {
    imprimirLinea(0, F("DATOS MANUAL"));
    if (lecturas.dhtValida) {
      imprimirLinea(1, String(F("T: ")) + String(lecturas.temperaturaC, 1) +
                           String(F(" C")));
      imprimirLinea(2, String(F("H: ")) + String(lecturas.humedadPct, 1) +
                           String(F(" %")));
    } else {
      imprimirLinea(1, F("T: --"));
      imprimirLinea(2, F("H: --"));
    }
    imprimirLinea(3, String(F("Gas: ")) + String(lecturas.gasAdc));
    imprimirLinea(4, String(F("Flama: ")) +
                         (lecturas.flamaDetectada ? String(F("FUEGO"))
                                                  : String(F("NO"))));
    imprimirLinea(5, String(F("Dist: ")) +
                         textoDistancia(servoDistanciaFrenteCm()) +
                         String(F(" cm")));
    return;
  }

  imprimirLinea(0, F("MODO MANUAL"));
  imprimirLinea(1, F("WiFi: OFF"));
  imprimirLinea(2, String(F("BT: ")) +
                       (controlBtConectado() ? controlBtNombre()
                                             : "SIN MANDO"));
  imprimirLinea(3, F("B: datos   Y: auto"));
  imprimirLinea(4, F("Servo centrado"));
  imprimirLinea(5, F("USB: manual/auto"));
}

void dibujarAuto() {
  imprimirLinea(0, F("MODO AUTOMATICO"));
  imprimirLinea(1, String(F("WiFi: ")) +
                       (wifiConectado() ? wifiIp() : String(F("conectando"))));
  imprimirLinea(2, F("BT: OFF"));
  imprimirLinea(3, String(F("Servo: ")) + String(servoAnguloActual()) +
                       String(F("  Dist: ")) +
                       textoDistancia(servoDistanciaFrenteCm()) +
                       String(F(" cm")));
  imprimirLinea(4, String(F("Obst: ")) +
                       (servoObstaculoFrente() ? String(F("SI"))
                                                : String(F("NO"))) +
                       String(F("  Libre: ")) + servoDireccionMasLibre());
  imprimirLinea(5, String(F("Telegram: ")) +
                       String(INTERVALO_TELEMETRIA_MS / 1000UL) +
                       String(F(" s")));
}

}  // namespace

void oledInit() {
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(400000);

  disponible = display.begin(SSD1306_SWITCHCAPVCC, OLED_DIRECCION);
  if (!disponible) {
    Serial.printf(
        "[OLED] No detectada en 0x%02X (SDA=GPIO%d, SCL=GPIO%d). "
        "Revisa la direccion y el cableado.\n",
        OLED_DIRECCION, PIN_OLED_SDA, PIN_OLED_SCL);
    return;
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.cp437(true);
  display.display();

  Serial.printf("[OLED] SSD1306 %dx%d lista en 0x%02X (SDA=GPIO%d, SCL=GPIO%d).\n",
                OLED_ANCHO, OLED_ALTO, OLED_DIRECCION, PIN_OLED_SDA,
                PIN_OLED_SCL);
  forzarRefresco = true;
}

void oledTick(const LecturasAmbientales& lecturas) {
  if (!disponible) {
    return;
  }

  const unsigned long ahora = millis();
  if (!forzarRefresco && (ahora - ultimoRefrescoMs < OLED_REFRESCO_MS)) {
    return;
  }

  ultimoRefrescoMs = ahora;
  forzarRefresco = false;

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  if (modoActual() == ModoOperacion::MANUAL) {
    dibujarManual(lecturas);
  } else {
    dibujarAuto();
  }

  display.display();
}

void oledMostrarDatos() {
  mostrarDatosHastaMs = millis() + OLED_TIEMPO_DATOS_MS;
  forzarRefresco = true;
}

void oledMostrarModo(ModoOperacion modo) {
  (void)modo;
  mostrarDatosHastaMs = 0;
  forzarRefresco = true;
}

bool oledDisponible() {
  return disponible;
}
