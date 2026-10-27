# 🤖 IndoorWatch — Rover IoT de Monitoreo Ambiental en Interiores

Firmware para un vehículo robótico autónomo basado en **ESP32** que monitorea
variables ambientales en interiores, envía alertas en tiempo real por
**Telegram** y, en fases posteriores, navegará evitando obstáculos y publicará
telemetría por **MQTT**.

> **Estado actual:** Etapa 1 implementada (sensores + Wi-Fi + Telegram) y
> control básico de los motores con L298N por Telegram y Monitor Serial.
> Pendiente: validación física, MQTT, ultrasonido, batería, evasión y mando
> Bluetooth.

- **Repositorio:** <https://github.com/Adrixn1140/rbtmvll>
- **Placa:** ESP32 DevKit (Arduino-ESP32 3.x)
- **Monitor Serial:** `115200` baudios
- **Bot:** `@IndoorWatch_IoT_bot`

---

## 1. Objetivos del proyecto

1. Leer de forma **no bloqueante** temperatura, humedad, gas/humo y flama.
2. Consultar el estado en tiempo real desde Telegram (`/estado`, `/sensores`).
3. Enviar **alertas automáticas** cuando una variable supere su umbral, con
   *cooldown* para no saturar el chat.
4. Publicar telemetría MQTT cada 10 s en una plataforma IoT (Etapa 2).
5. Navegar de forma autónoma y reactiva, evadiendo obstáculos, con batería
   monitoreada (Etapa 3).

---

## 2. Fases de desarrollo

| Etapa | Alcance | Estado |
| :---: | --- | :---: |
| **1** | DHT22 + MQ-2/MQ-135 + flama IR, Wi-Fi, comandos y alertas Telegram | ✅ Implementada |
| **2** | Telemetría MQTT cada 10 s + dashboard (Ubidots / Blynk / ThingSpeak) | ⏳ Pendiente |
| **3** | HC-SR04, divisor de batería, L298N, 2 motores DC y evasión reactiva | 🟡 Motores L298N implementados; percepción y evasión pendientes |

### Etapa 2 — tópicos MQTT previstos

Publicación cada **10 segundos**:

| Tópico | Dato | Unidad |
| --- | --- | --- |
| `rover/temp` | Temperatura | °C |
| `rover/hum` | Humedad relativa | % |
| `rover/gas` | Gas / humo | ADC 0–4095 |
| `rover/flama` | Flama detectada | `0` / `1` |
| `rover/bateria` | Nivel de batería | % |

### Etapa 3 — navegación reactiva

- Distancia **< 20 cm** → detener, retroceder, girar y reanudar la marcha.
- Batería **< 20 %** → alerta por Telegram.
- Control de tracción diferencial con L298N (dirección; PWM opcional).

---

## 3. Arquitectura del firmware

```text
                 ┌──────────────────────────────────────────┐
                 │                  ESP32                   │
                 │                                          │
  DHT22  ───────►│ GPIO 13  ┐                               │
  MQ-2   ───────►│ GPIO 34  ├──► sensores.cpp ──┐           │
  Flama  ───────►│ GPIO 35  ┘                    │           │
                 │                               ▼           │
                 │                        alertas.cpp        │
                 │                               │           │
                 │   wifi_manager.cpp ◄──────────┤           │
                 │        │                      ▼           │
                 │        └──────────────► telegram_bot.cpp │
                 └───────────────────────┬──────────────────┘
                                         │ Wi-Fi 2.4 GHz
                                         ▼
                                  Bot de Telegram
```

### Módulos

| Archivo | Responsabilidad |
| --- | --- |
| `rbtmvll.ino` | Orquestación del `setup()` y del `loop()` |
| `config.h` | Pines, umbrales, intervalos y constantes |
| `sensores.*` | Adquisición y validación de DHT22, MQ y flama |
| `wifi_manager.*` | Conexión y reconexión Wi-Fi no bloqueante |
| `telegram_bot.*` | Polling, comandos, respuestas y envío de mensajes |
| `alertas.*` | Evaluación de umbrales, cooldown y normalización |
| `motores.*` | Control no bloqueante del L298N, movimientos temporizados y demo |
| `secrets.h` | Credenciales reales (**ignorado por Git**) |
| `secrets.example.h` | Plantilla de credenciales |

### Regla de oro: código no bloqueante

Toda temporización se realiza con `millis()`. **No se usan `delay()` largos**.
Esto es indispensable para la Etapa 3: el lazo principal debe seguir atendiendo
el ultrasonido y los motores aunque se esté esperando el siguiente muestreo.

> Nota técnica: las librerías `DHT` y `UniversalTelegramBot` realizan esperas
> internas breves (lectura del sensor y handshake TLS). Se aceptan en la Etapa 1
> y se medirán antes de integrar la navegación autónoma.

---

## 4. Pinout oficial (ESP32)

### 4.1 Etapa 1 — sensores ambientales

| Componente | Señal | GPIO | Alimentación | Notas |
| --- | --- | :---: | :---: | --- |
| **DHT22** | DATA | **13** | 3.3 V | Requiere pull-up de 10 kΩ a 3.3 V (muchos módulos lo incluyen) |
| **MQ-2 / MQ-135** | AO | **34** | 5 V (VIN) | ADC1, solo entrada. Línea base ≈ 2000 |
| **Flama IR (LM393)** | DO / OUT | **35** | 3.3 V | ADC1, solo entrada. `LOW` = fuego, `HIGH` = normal |

> ⚠️ **Protección del ADC:** el módulo MQ se alimenta a 5 V y su salida analógica
> puede acercarse a 5 V en concentraciones altas. En el montaje final es
> **obligatorio** el divisor de 10 kΩ / 20 kΩ antes de GPIO 34. Durante las
> pruebas actuales se usa la conexión directa ya existente; no exponer el pin a
> más de 3.3 V.

### 4.2 Etapa 3 — percepción, energía y locomoción

| Componente | Señal | GPIO | Notas |
| --- | --- | :---: | --- |
| **Divisor de batería** | Punto medio | **36** | 2× 18650 en serie (7.4 V nominal / 8.4 V máx), R 20 kΩ + 10 kΩ. GPIO 36 es solo entrada |
| **HC-SR04** | TRIG | **32** | Salida de disparo |
| **HC-SR04** | ECHO | **33** | Divisor 1 kΩ / 2 kΩ para bajar 5 V → 3.3 V |
| **L298N** | IN1 | **18** | Dirección motor A |
| **L298N** | IN2 | **19** | Dirección motor A |
| **L298N** | IN3 | **22** | Dirección motor B |
| **L298N** | IN4 | **23** | Dirección motor B |
| **L298N** | ENA (PWM) | **25** | Velocidad motor A (opcional; requiere quitar el jumper ENA) |
| **L298N** | ENB (PWM) | **26** | Velocidad motor B (opcional; requiere quitar el jumper ENB) |

Criterios del mapa de pines:

- Entradas analógicas en **ADC1** (GPIO 32–39) para que el Wi-Fi no interfiera.
- **GPIO 12 evitado** por ser pin de arranque (*strapping*).
- GPIO 34, 35 y 36 son **solo entrada**, sin pull-up interno.
- El HC-SR04 ocupa GPIO 32 (TRIG) y GPIO 33 (ECHO); por eso la batería se movió a **GPIO 36** y ENB a **GPIO 26**.
- El ECHO del HC-SR04 entrega 5 V: es **obligatorio** el divisor 1 kΩ / 2 kΩ antes de GPIO 33.
- Alimentación de motores **independiente** de la ESP32, con **GND común**.

### 4.3 Alimentación del L298N

| Terminal L298N | Conexión |
| --- | --- |
| **+12V / VMS** | Positivo del pack 2×18650 (7.4–8.4 V) |
| **GND** | Negativo de la batería **y** GND de la ESP32 |
| **+5V** | Salida del regulador de 5 V del módulo; **no** conectar al pin 3.3 V de la ESP32 |
| **OUT1 / OUT2** | Motor A |
| **OUT3 / OUT4** | Motor B |

> El L298N **no se alimenta desde la ESP32**. La ESP32 solo entrega las señales
> IN1–IN4 y, si se usa PWM, ENA/ENB. La potencia de los motores va a la batería.
> Si la ESP32 se alimenta por USB durante las pruebas, su GND debe quedar unido
> al GND del L298N y al negativo de la batería.
>
> Para 2×18650 se puede dejar puesto el jumper de 5 V del L298N. Si se alimenta
> con más de 12 V, se debe quitar ese jumper y entregar 5 V externos al terminal
> **+5V**. Para el montaje final se recomienda un convertidor reductor de 5 V
> independiente para la ESP32, no la salida de 5 V del L298N.

### 4.4 Prueba de motores

1. Coloca el rover **sobre bloques** para que las ruedas giren sin desplazarse.
2. Verifica que los jumpers **ENA** y **ENB** estén puestos.
3. Conecta la alimentación del L298N y une el GND con la ESP32.
4. En Telegram, envía `/demo` para ejecutar la secuencia completa.
5. También puedes usar el Monitor Serial:
   - `a` = adelante
   - `r` = atrás
   - `i` = izquierda
   - `d` = derecha
   - `s` = detener
   - `p` = demo

Los movimientos manuales duran **1.5 s** y se detienen solos. `/detener` corta
cualquier movimiento de inmediato.

Si un motor gira al revés, cambia `MOTOR_A_INVERTIDO` o `MOTOR_B_INVERTIDO` a
`true` en `config.h`, o intercambia los dos cables de ese motor en el L298N.

> En esta etapa el control es **solo de dirección**, a velocidad fija. Para usar
> PWM habrá que retirar los jumpers ENA/ENB y conectarlos a GPIO 25 y 26.

---

## 5. Umbrales y calibración

| Variable | Condición de alerta | Valor |
| --- | --- | --- |
| Temperatura | `temperatura >` | **40.0 °C** |
| Gas / humo | `gasAdc >` | **2700** (ADC 12 bits) |
| Flama IR | `digitalRead() ==` | **LOW** (fuego) |
| Batería (Etapa 3) | `bateria <` | **20 %** |
| Obstáculo (Etapa 3) | `distancia <` | **20 cm** |

La línea base del MQ en aire limpio se estabiliza alrededor de **2000** con el
módulo alimentado a 5 V. El umbral de **2700** deja un margen para evitar falsos
positivos. El MQ requiere un precalentamiento inicial (decenas de segundos, y
para mayor precisión, varias horas); si el entorno cambia, recalibrar
`UMBRAL_GAS_ADC` en `config.h`.

> ⚠️ **Importante para la Etapa 3:** al instalar el divisor de 10 kΩ / 20 kΩ, la
> tensión que llega al ADC se reduce a **2/3**. La línea base de ~2000 bajará a
> ~1333 y el umbral de 2700 deberá recalibrarse (referencia inicial: ~1800).
> El valor **2700 corresponde a la conexión directa actual**, sin divisor.

La lectura de gas mostrada y evaluada es el **promedio de 8 muestras ADC**; el
valor crudo de la última muestra se incluye en el Monitor Serial.

---

## 6. Bot de Telegram

### Comandos

| Comando | Acción |
| --- | --- |
| `/start` | Bienvenida y ayuda |
| `/estado` | Lecturas actuales, edad de la muestra, Wi-Fi y uptime |
| `/sensores` | Alias de `/estado` |
| `/umbrales` | Límites de alerta configurados |
| `/ping` | Uptime, RSSI, IP y memoria libre |
| `/motores` | Ayuda del control de motores |
| `/demo` | Secuencia automática: adelante, atrás, izquierda y derecha |
| `/adelante` | Avanzar durante 1.5 s |
| `/atras` | Retroceder durante 1.5 s |
| `/izquierda` | Girar a la izquierda durante 1.5 s |
| `/derecha` | Girar a la derecha durante 1.5 s |
| `/detener` | Parar los motores inmediatamente |
| `/id` | Chat ID autorizado |
| `/ayuda` | Lista de comandos |

Solo se atienden mensajes del `CHAT_ID` configurado. Cualquier otro chat se
ignora y se registra en el Monitor Serial.

### Comportamiento de las alertas

1. Si aparece una condición crítica, se envía una alerta **inmediatamente**.
2. Si la condición cambia (por ejemplo, se suma gas a una alerta de
   temperatura), se envía una actualización inmediata.
3. Mientras la alerta siga activa, se repite como máximo **cada 60 s**
   (`COOLDOWN_ALERTA_MS`).
4. Cuando todo vuelve a la normalidad se envía un mensaje de **valores
   normalizados**.
5. Si un envío falla (Wi-Fi caído, TLS, etc.), se reintenta cada **10 s** sin
   bloquear el lazo principal.
6. Si el DHT22 falla, se conserva la última condición conocida para no declarar
   una normalización falsa; el mensaje indica que no hay lectura actual.

---

## 7. Estructura del repositorio

```text
rbtmvll/
├── rbtmvll.ino             # Sketch principal (setup / loop)
├── config.h                # Pines, umbrales y tiempos
├── sensores.h / .cpp       # DHT22, MQ-2/MQ-135 y flama IR
├── wifi_manager.h / .cpp   # Wi-Fi no bloqueante y reconexión
├── telegram_bot.h / .cpp   # Bot, comandos y respuestas
├── alertas.h / .cpp        # Umbrales, cooldown y normalización
├── motores.h / .cpp        # L298N, movimientos y secuencia de prueba
├── secrets.example.h       # Plantilla de credenciales (sí se versiona)
├── secrets.h               # Credenciales reales (IGNORADO por Git)
├── .gitignore
└── README.md
```

> Arduino IDE compila todos los `.cpp`/`.h` que estén en la misma carpeta del
> sketch. Mantener esa estructura para conservar la compatibilidad.

---

## 8. Puesta en marcha

### 8.1 Requisitos de software

- Arduino IDE 2.x.
- Núcleo **ESP32 by Espressif Systems** (probado con `3.3.12`).
- Bibliotecas (Library Manager):
  - **DHT sensor library** de Adafruit (`1.4.7` o superior).
  - **Adafruit Unified Sensor** (dependencia de la anterior).
  - **UniversalTelegramBot** de Brian Lough (`1.3.0` o superior).
  - **ArduinoJson** (`7.x`).

### 8.2 Configurar credenciales

1. Copiar `secrets.example.h` a `secrets.h` (misma carpeta del sketch).
2. Completar:
   - `WIFI_SSID` y `WIFI_PASSWORD` (red **2.4 GHz**).
   - `BOT_TOKEN`: token entregado por `@BotFather`.
   - `CHAT_ID`: ID numérico obtenido con `@userinfobot`.
3. Verificar que Git ignore el archivo:

```bash
git check-ignore -v secrets.h
# .gitignore:5:secrets.h    secrets.h
```

### 8.3 Compilar y cargar

Desde Arduino IDE:

1. Abrir `rbtmvll.ino`.
2. Seleccionar placa **ESP32 Dev Module** y el puerto COM correspondiente.
3. **Verificar** y luego **Subir**.
4. Abrir el Monitor Serial a **115200 baudios**.

Desde Arduino CLI:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p COM7 --fqbn esp32:esp32:esp32 .
```

### 8.4 Salida esperada del Monitor Serial

```text
==============================================
 IndoorWatch - Rover IoT de monitoreo
 Etapa 1 + control de motores L298N
==============================================
[SENSORES] DHT22=GPIO13, MQ=GPIO34, FLAMA=GPIO35
[MOTORES] L298N listo: IN1=18 IN2=19 IN3=22 IN4=23. ENA/ENB con jumpers (velocidad fija).
[MOTORES] Serial: a=adelante r=atras i=izquierda d=derecha s=detener p=demo
[WIFI] Conectando a la red: MiRed
[SISTEMA] Listo. Monitor Serial a 115200 baudios.
[WIFI] Conectado. IP: 192.168.1.50 | RSSI: -62 dBm
[TELEGRAM] Aviso de inicio enviado correctamente.
[SENSOR] T=25.4 C | H=58.7 % | GAS=1987 (crudo=2003) | FLAMA=NO
```

---

## 9. Verificación funcional (checklist)

- [ ] El Monitor Serial muestra lecturas válidas cada ~2.5 s.
- [ ] `/estado` responde con temperatura, humedad, gas y flama.
- [ ] `/umbrales` muestra los límites configurados.
- [ ] Solo el chat autorizado recibe respuestas.
- [ ] Un mensaje de un chat desconocido se ignora.
- [ ] Al acercar un encendedor (sin llama) al MQ, `GAS` sube y llega la alerta.
- [ ] Al tapar el DHT22 o calentarlo suavemente por encima de 40 °C, llega la
      alerta (usar calor controlado, sin dañar el sensor).
- [ ] Al iluminar el sensor de flama, se envía la alerta de fuego.
- [ ] Al retirar la condición crítica, llega el mensaje de normalización.
- [ ] La alerta no se repite antes de 60 s mientras la condición persiste.
- [ ] Al desconectar el router, la ESP32 reintenta y se recupera al volver.
- [ ] Con el rover sobre bloques, `/demo` mueve ambos motores en ambos sentidos.
- [ ] `/detener` detiene los motores inmediatamente.
- [ ] Los comandos `a`, `r`, `i`, `d`, `s` y `p` funcionan por Monitor Serial.

> Para probar alertas sin fuego real: usar una fuente de luz IR o un
> encendedor **sin encender** para gas; nunca exponer los sensores a llamas
> abiertas ni a humo dentro de una habitación cerrada.

---

## 10. Seguridad

- `secrets.h` está en `.gitignore`; **nunca** hacer `git add -f secrets.h`.
- El token del bot fue compartido durante el desarrollo: si el repositorio o el
  chat pudieran haberse expuesto, **revocarlo en `@BotFather`** (`/revoke`) y
  actualizar `secrets.h`.
- El bot solo responde al `CHAT_ID` configurado; no se aceptan comandos de
  terceros.
- `WiFiClientSecure::setInsecure()` se usa para simplificar el prototipo, pero
  **no valida la cadena de certificados**. Para una versión de producción,
  fijar el certificado raíz de Telegram.
- No alimentar los motores desde la ESP32 ni desde el regulador de 3.3 V.
  Usar una fuente/pack de baterías adecuado y **GND común**.
- El módulo MQ necesita el divisor de tensión definitivo antes de conectar su
  salida al ADC de la ESP32.
- Antes de probar los motores, colocar el rover sobre bloques. Los comandos
  `/demo`, `/adelante`, `/atras`, `/izquierda` y `/derecha` pueden moverlo.
- Los jumpers ENA/ENB deben estar puestos si no se usa PWM; de lo contrario los
  motores no recibirán habilitación.

---

## 11. Flujo de trabajo Git

```bash
git status
git add <archivos>
git commit -m "tipo: descripción"
git push origin main
```

- Commits atómicos y descriptivos (`feat`, `fix`, `docs`, `chore`, `refactor`).
- Antes de cada commit: `git status` para confirmar que `secrets.h` no aparece.
- La rama principal es `main`.

---

## 12. Próximos pasos

1. Validar los motores en hardware, ajustar el sentido de giro y confirmar que
   ambos se detienen correctamente.
2. Integrar el HC-SR04 y el divisor de batería; validar el ECHO con su divisor.
3. Implementar la máquina de estados de evasión: `AVANZAR → DETENER →
   RETROCEDER → GIRAR → AVANZAR`.
4. Agregar el módulo MQTT (Etapa 2) manteniendo la arquitectura no bloqueante:
   publicación cada 10 s a `rover/temp`, `rover/hum`, `rover/gas`,
   `rover/flama` y `rover/bateria`.
5. Integrar el mando GameSir Nova Lite con Bluepad32.
6. Añadir pruebas de integración y registro de datos para el dashboard.

---

<p align="center">Proyecto académico de sistemas embebidos, IoT y robótica · ESP32</p>
