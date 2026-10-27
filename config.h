#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ==================================================================
//  IndoorWatch - Configuración central del firmware
//  Rover IoT de Monitoreo Ambiental en Interiores
// ==================================================================

// ------------------------------------------------------------------
// Pines oficiales - Etapa 1 (ya cableados en protoboard)
// ------------------------------------------------------------------
constexpr uint8_t PIN_DHT22     = 13;  // DHT22 DATA, alimentado a 3.3 V
constexpr uint8_t PIN_MQ_ANALOG = 34;  // MQ-2/MQ-135 AO, ADC1 (solo entrada)
constexpr uint8_t PIN_FLAMA     = 35;  // Flama IR DO, ADC1 (solo entrada)

// ------------------------------------------------------------------
// Umbrales de alerta (calibrados en campo)
// ------------------------------------------------------------------
constexpr float   UMBRAL_TEMP_C  = 40.0f;  // Alerta si temperatura > 40.0 °C
constexpr int     UMBRAL_GAS_ADC = 2700;   // Línea base ~2000 en aire limpio
constexpr uint8_t FLAMA_NIVEL_ACTIVO = LOW; // LM393: LOW = fuego detectado

// ------------------------------------------------------------------
// Tiempos no bloqueantes (millis). No usar delay() en el loop.
// ------------------------------------------------------------------
constexpr unsigned long INTERVALO_SENSORES_MS        = 2500UL;  // DHT22 >= 2 s
constexpr unsigned long INTERVALO_TELEGRAM_MS        = 3000UL;
constexpr unsigned long COOLDOWN_ALERTA_MS           = 60000UL; // Repetición
constexpr unsigned long INTERVALO_RECONEXION_WIFI_MS = 15000UL;
constexpr unsigned long INTERVALO_REINTENTO_ENVIO_MS = 10000UL;

// ------------------------------------------------------------------
// Adquisición y validación
// ------------------------------------------------------------------
constexpr uint8_t MUESTRAS_GAS     = 8;
constexpr float   TEMP_MIN_VALIDA_C = -40.0f;
constexpr float   TEMP_MAX_VALIDA_C = 80.0f;

// ------------------------------------------------------------------
// Etapa 3 (documentada, NO inicializada todavía)
//
// Batería (2x 18650, divisor 20 kΩ / 10 kΩ) .... GPIO 32 (ADC1)
// HC-SR04: TRIG ............................... GPIO 5
// HC-SR04: ECHO (divisor 1 kΩ / 2 kΩ) ......... GPIO 21
// L298N: IN1 / IN2 / IN3 / IN4 ................ GPIO 18 / 19 / 22 / 23
// L298N: ENA (PWM) / ENB (PWM) ................ GPIO 25 / 33 (opcional)
//
// GPIO 18 se reasignó al L298N; por eso el ECHO del HC-SR04 se movió
// a GPIO 21 para evitar el conflicto.
//
// Se dejan fuera de config.h para no energizar accidentalmente los
// motores durante las pruebas de la Etapa 1.
// ------------------------------------------------------------------

#endif
