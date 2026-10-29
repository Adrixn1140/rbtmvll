#include "motores.h"
#include "config.h"

namespace {

struct PasoDemo {
  DireccionMovimiento direccion;
  unsigned long duracionMs;
};

// Secuencia corta para comprobar ambos motores y ambos sentidos.
const PasoDemo SECUENCIA_DEMO[] = {
    {DireccionMovimiento::ADELANTE, 1200UL},
    {DireccionMovimiento::DETENIDO, 400UL},
    {DireccionMovimiento::ATRAS, 1200UL},
    {DireccionMovimiento::DETENIDO, 400UL},
    {DireccionMovimiento::IZQUIERDA, 900UL},
    {DireccionMovimiento::DETENIDO, 400UL},
    {DireccionMovimiento::DERECHA, 900UL},
    {DireccionMovimiento::DETENIDO, 600UL},
};

constexpr uint8_t TOTAL_PASOS_DEMO =
    sizeof(SECUENCIA_DEMO) / sizeof(SECUENCIA_DEMO[0]);

DireccionMovimiento direccionActual = DireccionMovimiento::DETENIDO;
unsigned long finMovimientoMs = 0;
unsigned long finControlContinuoMs = 0;
bool movimientoTemporalActivo = false;
bool controlContinuoActivo = false;

bool demoActiva = false;
uint8_t pasoDemo = 0;
unsigned long finPasoDemoMs = 0;

bool tiempoVencido(unsigned long ahora, unsigned long objetivo) {
  // Funciona también cuando millis() da la vuelta.
  return static_cast<long>(ahora - objetivo) >= 0;
}

void aplicarMotor(uint8_t pinAvance, uint8_t pinRetroceso, bool avance,
                  bool invertido) {
  if (invertido) {
    avance = !avance;
  }
  digitalWrite(pinAvance, avance ? HIGH : LOW);
  digitalWrite(pinRetroceso, avance ? LOW : HIGH);
}

void aplicarDireccion(DireccionMovimiento direccion) {
  switch (direccion) {
    case DireccionMovimiento::ADELANTE:
      aplicarMotor(PIN_L298N_IN1, PIN_L298N_IN2, true, MOTOR_A_INVERTIDO);
      aplicarMotor(PIN_L298N_IN3, PIN_L298N_IN4, true, MOTOR_B_INVERTIDO);
      break;

    case DireccionMovimiento::ATRAS:
      aplicarMotor(PIN_L298N_IN1, PIN_L298N_IN2, false, MOTOR_A_INVERTIDO);
      aplicarMotor(PIN_L298N_IN3, PIN_L298N_IN4, false, MOTOR_B_INVERTIDO);
      break;

    case DireccionMovimiento::IZQUIERDA:
      // Giro sobre el centro: motor A atrás, motor B adelante.
      aplicarMotor(PIN_L298N_IN1, PIN_L298N_IN2, false, MOTOR_A_INVERTIDO);
      aplicarMotor(PIN_L298N_IN3, PIN_L298N_IN4, true, MOTOR_B_INVERTIDO);
      break;

    case DireccionMovimiento::DERECHA:
      // Giro sobre el centro: motor A adelante, motor B atrás.
      aplicarMotor(PIN_L298N_IN1, PIN_L298N_IN2, true, MOTOR_A_INVERTIDO);
      aplicarMotor(PIN_L298N_IN3, PIN_L298N_IN4, false, MOTOR_B_INVERTIDO);
      break;

    case DireccionMovimiento::DETENIDO:
    default:
      digitalWrite(PIN_L298N_IN1, LOW);
      digitalWrite(PIN_L298N_IN2, LOW);
      digitalWrite(PIN_L298N_IN3, LOW);
      digitalWrite(PIN_L298N_IN4, LOW);
      break;
  }

  direccionActual = direccion;
}

void iniciarPasoDemo() {
  const PasoDemo& paso = SECUENCIA_DEMO[pasoDemo];
  aplicarDireccion(paso.direccion);
  finPasoDemoMs = millis() + paso.duracionMs;

  Serial.printf("[MOTORES] Demo paso %u/%u: %s durante %lu ms\n",
                static_cast<unsigned>(pasoDemo + 1),
                static_cast<unsigned>(TOTAL_PASOS_DEMO),
                motoresEstadoTexto(), paso.duracionMs);
}

}  // namespace

void motoresInit() {
  pinMode(PIN_L298N_IN1, OUTPUT);
  pinMode(PIN_L298N_IN2, OUTPUT);
  pinMode(PIN_L298N_IN3, OUTPUT);
  pinMode(PIN_L298N_IN4, OUTPUT);

  // ENA/ENB no se tocan: en el cableado actual quedan con sus jumpers
  // puestos, por lo que los motores funcionan a velocidad fija.
  aplicarDireccion(DireccionMovimiento::DETENIDO);

  Serial.printf(
      "[MOTORES] L298N listo: IN1=%d IN2=%d IN3=%d IN4=%d. "
      "ENA/ENB con jumpers (velocidad fija).\n",
      PIN_L298N_IN1, PIN_L298N_IN2, PIN_L298N_IN3, PIN_L298N_IN4);
  Serial.println(
      "[MOTORES] USB/Bluetooth: a=adelante r=atras i=izquierda d=derecha "
      "s=detener p=demo");
}

void motoresMover(DireccionMovimiento direccion, unsigned long duracionMs) {
  demoActiva = false;
  controlContinuoActivo = false;
  finControlContinuoMs = 0;

  if (direccion == DireccionMovimiento::DETENIDO) {
    motoresDetener();
    return;
  }

  aplicarDireccion(direccion);
  movimientoTemporalActivo = true;
  finMovimientoMs = millis() + duracionMs;

  Serial.printf("[MOTORES] %s durante %lu ms\n", motoresEstadoTexto(),
                duracionMs);
}

void motoresControlContinuo(DireccionMovimiento direccion) {
  demoActiva = false;
  movimientoTemporalActivo = false;
  controlContinuoActivo = true;

  // Cada orden refresca el paro de seguridad, incluso si la dirección no
  // cambia. Así el rover no se queda avanzando si se pierde el enlace.
  finControlContinuoMs = millis() + DURACION_CONTROL_CONTINUO_MS;

  if (direccion == direccionActual) {
    return;
  }

  aplicarDireccion(direccion);
  Serial.printf("[MOTORES] Control continuo: %s\n", motoresEstadoTexto());
}

void motoresDetener() {
  const bool estabaActivo =
      demoActiva || movimientoTemporalActivo || controlContinuoActivo ||
      direccionActual != DireccionMovimiento::DETENIDO;

  demoActiva = false;
  movimientoTemporalActivo = false;
  controlContinuoActivo = false;
  finControlContinuoMs = 0;
  aplicarDireccion(DireccionMovimiento::DETENIDO);

  if (estabaActivo) {
    Serial.println(F("[MOTORES] Detenido."));
  }
}

void motoresDemo() {
  demoActiva = true;
  movimientoTemporalActivo = false;
  controlContinuoActivo = false;
  finControlContinuoMs = 0;
  pasoDemo = 0;
  Serial.println(F("[MOTORES] Iniciando secuencia de demostracion."));
  iniciarPasoDemo();
}

void motoresTick() {
  const unsigned long ahora = millis();

  if (demoActiva) {
    if (!tiempoVencido(ahora, finPasoDemoMs)) {
      return;
    }

    ++pasoDemo;
    if (pasoDemo >= TOTAL_PASOS_DEMO) {
      demoActiva = false;
      aplicarDireccion(DireccionMovimiento::DETENIDO);
      Serial.println(F("[MOTORES] Demostracion terminada."));
      return;
    }

    iniciarPasoDemo();
    return;
  }

  if (controlContinuoActivo &&
      tiempoVencido(ahora, finControlContinuoMs)) {
    controlContinuoActivo = false;
    finControlContinuoMs = 0;
    aplicarDireccion(DireccionMovimiento::DETENIDO);
    Serial.println(
        F("[MOTORES] Sin ordenes de control continuo; detenido."));
    return;
  }

  if (movimientoTemporalActivo && tiempoVencido(ahora, finMovimientoMs)) {
    movimientoTemporalActivo = false;
    aplicarDireccion(DireccionMovimiento::DETENIDO);
    Serial.println(F("[MOTORES] Movimiento terminado."));
  }
}

bool motoresDemoActiva() {
  return demoActiva;
}

const char* motoresEstadoTexto() {
  switch (direccionActual) {
    case DireccionMovimiento::ADELANTE:
      return "ADELANTE";
    case DireccionMovimiento::ATRAS:
      return "ATRAS";
    case DireccionMovimiento::IZQUIERDA:
      return "IZQUIERDA";
    case DireccionMovimiento::DERECHA:
      return "DERECHA";
    case DireccionMovimiento::DETENIDO:
    default:
      return "DETENIDO";
  }
}
