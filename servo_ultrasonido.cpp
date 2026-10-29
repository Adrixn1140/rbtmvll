#include "servo_ultrasonido.h"

#include <ESP32Servo.h>
#include <math.h>

#include "config.h"

namespace {

Servo servo;

// Ángulos del barrido: izquierda, frente, derecha.
const uint8_t ANGULOS_ESCANEO[3] = {SERVO_ANGULO_MIN, SERVO_ANGULO_CENTRO,
                                    SERVO_ANGULO_MAX};
const char* NOMBRES_ANGULO[3] = {"IZQUIERDA", "FRENTE", "DERECHA"};

enum class EstadoMedicion : uint8_t {
  REPOSO,
  MOVIENDO,
  ESPERANDO_ECO_ALTO,
  ESPERANDO_ECO_BAJO,
  PAUSA
};

EstadoMedicion estado = EstadoMedicion::REPOSO;
float distanciasCm[3] = {NAN, NAN, NAN};

uint8_t indice = 1;  // 0=izquierda, 1=frente, 2=derecha
uint8_t anguloActual = SERVO_ANGULO_CENTRO;
bool escanerActivo = false;

unsigned long finMovimientoMs = 0;
unsigned long finPausaMs = 0;
unsigned long inicioPulsoUs = 0;
unsigned long ecoInicioUs = 0;
unsigned long ultimaMedicionMs = 0;

void moverA(uint8_t angulo) {
  servo.write(angulo);
  anguloActual = angulo;
  estado = EstadoMedicion::MOVIENDO;
  finMovimientoMs = millis() + SERVO_ASENTADO_MS;
}

void iniciarPulso() {
  digitalWrite(PIN_HCSR04_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_HCSR04_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_HCSR04_TRIG, LOW);

  inicioPulsoUs = micros();
  estado = EstadoMedicion::ESPERANDO_ECO_ALTO;
}

void finalizarMedicion(float distanciaCm) {
  distanciasCm[indice] = distanciaCm;
  ultimaMedicionMs = millis();

  if (distanciaCm > 0.0f && !isnan(distanciaCm)) {
    Serial.printf("[SONAR] %s: %.1f cm\n", NOMBRES_ANGULO[indice],
                  distanciaCm);
  } else {
    Serial.printf("[SONAR] %s: sin eco\n", NOMBRES_ANGULO[indice]);
  }

  if (escanerActivo) {
    indice = (indice + 1) % 3;
    estado = EstadoMedicion::PAUSA;
    finPausaMs = millis() + SERVO_PAUSA_MS;
  } else {
    indice = 1;
    estado = EstadoMedicion::REPOSO;
  }
}

void tickEstado() {
  switch (estado) {
    case EstadoMedicion::REPOSO:
      if (!escanerActivo &&
          (millis() - ultimaMedicionMs >= SERVO_MEDICION_MANUAL_MS)) {
        iniciarPulso();
      }
      break;

    case EstadoMedicion::MOVIENDO:
      if (static_cast<long>(millis() - finMovimientoMs) >= 0) {
        iniciarPulso();
      }
      break;

    case EstadoMedicion::ESPERANDO_ECO_ALTO:
      if (digitalRead(PIN_HCSR04_ECHO) == HIGH) {
        ecoInicioUs = micros();
        estado = EstadoMedicion::ESPERANDO_ECO_BAJO;
      } else if (micros() - inicioPulsoUs >= ULTRASONIDO_TIMEOUT_US) {
        finalizarMedicion(NAN);
      }
      break;

    case EstadoMedicion::ESPERANDO_ECO_BAJO:
      if (digitalRead(PIN_HCSR04_ECHO) == LOW) {
        const unsigned long duracionUs = micros() - ecoInicioUs;
        finalizarMedicion(static_cast<float>(duracionUs) / 58.0f);
      } else if (micros() - ecoInicioUs >= ULTRASONIDO_TIMEOUT_US) {
        finalizarMedicion(NAN);
      }
      break;

    case EstadoMedicion::PAUSA:
      if (static_cast<long>(millis() - finPausaMs) >= 0) {
        if (escanerActivo) {
          moverA(ANGULOS_ESCANEO[indice]);
        } else {
          estado = EstadoMedicion::REPOSO;
        }
      }
      break;
  }
}

}  // namespace

void servoUltrasonidoInit() {
  pinMode(PIN_HCSR04_TRIG, OUTPUT);
  pinMode(PIN_HCSR04_ECHO, INPUT);
  digitalWrite(PIN_HCSR04_TRIG, LOW);

  servo.setPeriodHertz(50);
  servo.attach(PIN_SERVO, 500, 2400);
  servo.write(SERVO_ANGULO_CENTRO);

  anguloActual = SERVO_ANGULO_CENTRO;
  indice = 1;
  escanerActivo = false;
  estado = EstadoMedicion::MOVIENDO;
  finMovimientoMs = millis() + SERVO_ASENTADO_MS;

  Serial.printf(
      "[SERVO] SG90 en GPIO%d. HC-SR04 TRIG=GPIO%d ECHO=GPIO%d. "
      "Barrido %d-%d grados.\n",
      PIN_SERVO, PIN_HCSR04_TRIG, PIN_HCSR04_ECHO, SERVO_ANGULO_MIN,
      SERVO_ANGULO_MAX);
}

void servoUltrasonidoTick(bool escanear) {
  if (escanear && !escanerActivo) {
    escanerActivo = true;
    indice = 0;
    moverA(ANGULOS_ESCANEO[indice]);
  } else if (!escanear && escanerActivo) {
    escanerActivo = false;
    indice = 1;
    moverA(SERVO_ANGULO_CENTRO);
  }

  tickEstado();
}

void servoUltrasonidoCentrar() {
  escanerActivo = false;
  indice = 1;
  moverA(SERVO_ANGULO_CENTRO);
}

float servoDistanciaCm(uint8_t posicion) {
  if (posicion >= 3) {
    return NAN;
  }
  return distanciasCm[posicion];
}

float servoDistanciaFrenteCm() {
  return distanciasCm[1];
}

float servoDistanciaIzquierdaCm() {
  return distanciasCm[0];
}

float servoDistanciaDerechaCm() {
  return distanciasCm[2];
}

bool servoObstaculoFrente() {
  const float frente = distanciasCm[1];
  return !isnan(frente) && frente > 0.0f && frente < UMBRAL_OBSTACULO_CM;
}

uint8_t servoAnguloActual() {
  return anguloActual;
}

uint8_t servoIndiceActual() {
  return indice;
}

bool servoEscanerActivo() {
  return escanerActivo;
}

const char* servoDireccionMasLibre() {
  const float izquierda = distanciasCm[0];
  const float derecha = distanciasCm[2];
  const bool izquierdaValida = !isnan(izquierda) && izquierda > 0.0f;
  const bool derechaValida = !isnan(derecha) && derecha > 0.0f;

  if (!izquierdaValida && !derechaValida) {
    return "SIN DATOS";
  }
  if (izquierdaValida && (!derechaValida || izquierda > derecha + 5.0f)) {
    return "IZQUIERDA";
  }
  if (derechaValida && (!izquierdaValida || derecha > izquierda + 5.0f)) {
    return "DERECHA";
  }
  return "FRENTE";
}
