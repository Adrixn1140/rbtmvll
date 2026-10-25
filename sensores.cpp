#include "sensores.h"
#include "config.h"

#include <DHT.h>
#include <math.h>

namespace {

DHT dht(PIN_DHT22, DHT22);
LecturasAmbientales lecturas;
unsigned long ultimaLecturaMs = 0;

// Promedia varias muestras del ADC para reducir ruido sin bloquear el lazo.
int leerGasPromediado(int& crudo) {
  long suma = 0;
  for (uint8_t i = 0; i < MUESTRAS_GAS; ++i) {
    crudo = analogRead(PIN_MQ_ANALOG);
    suma += crudo;
  }
  return static_cast<int>(suma / MUESTRAS_GAS);
}

void leerDht22() {
  const float t = dht.readTemperature();
  const float h = dht.readHumidity();

  const bool valida = !isnan(t) && !isnan(h) &&
                      t >= TEMP_MIN_VALIDA_C && t <= TEMP_MAX_VALIDA_C &&
                      h >= 0.0f && h <= 100.0f;

  if (!valida) {
    // Se conservan los últimos valores válidos, pero se marca la lectura
    // como no confiable para no generar falsas normalizaciones.
    lecturas.dhtValida = false;
    return;
  }

  lecturas.temperaturaC = t;
  lecturas.humedadPct = h;
  lecturas.dhtValida = true;
}

}  // namespace

void sensoresInit() {
  pinMode(PIN_FLAMA, INPUT);
  pinMode(PIN_MQ_ANALOG, INPUT);

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_MQ_ANALOG, ADC_11db);

  dht.begin();

  // La primera lectura se hará después de INTERVALO_SENSORES_MS para dar
  // tiempo de estabilización al DHT22.
  ultimaLecturaMs = millis();

  Serial.println(F("[SENSORES] DHT22=GPIO4, MQ=GPIO34, FLAMA=GPIO35"));
}

bool sensoresTick() {
  const unsigned long ahora = millis();
  if (ahora - ultimaLecturaMs < INTERVALO_SENSORES_MS) {
    return false;
  }
  ultimaLecturaMs = ahora;

  int crudo = 0;
  lecturas.gasAdc = leerGasPromediado(crudo);
  lecturas.gasAdcCrudo = crudo;
  lecturas.flamaDetectada = (digitalRead(PIN_FLAMA) == FLAMA_NIVEL_ACTIVO);

  leerDht22();
  lecturas.ultimaActualizacionMs = ahora;
  return true;
}

const LecturasAmbientales& sensoresLecturas() {
  return lecturas;
}
