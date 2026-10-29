# 🤖 IndoorWatch — Rover IoT de Monitoreo Ambiental en Interiores

Firmware para un vehículo robótico basado en **ESP32** que monitorea variables
ambientales en interiores, envía alertas por **Telegram** y se controla desde el
celular mediante un **módulo Bluetooth serial** (HC-05 / HC-06 / HM-10).

> **Estado actual:** Etapa 1 implementada (sensores + Wi-Fi + Telegram), motores
> L298N y control por Bluetooth serial implementados. Pendiente: validación
> física, MQTT, ultrasonido, lectura de batería y evasión de obstáculos.
>
> **Importante:** se eliminó la integración directa con el mando GameSir y
> Bluepad32. Ahora el control es: **celular → módulo Bluetooth serial → ESP32**.
> Un HC-05/06/HM-10 **no** puede conectarse directamente al mando GameSir porque
> no es un host HID; solo sirve como puente serial para el teléfono.

- **Repositorio:** <https://github.com/Adrixn1140/rbtmvll>
- **Placa:** ESP32 DevKit clásica, chip detectado **ESP32-D0WD-V3**
- **Core:** Arduino-ESP32 **3.3.12** (`esp32:esp32:esp32`)
- **Monitor Serial USB:** `115200` baudios
- **Bluetooth serial:** `9600` baudios, 8N1
- **Bot:** `@IndoorWatch_IoT_bot`

---

## 1. Objetivos del proyecto

1. Leer de forma **no bloqueante** temperatura, humedad, gas/humo y flama.
2. Consultar el estado en tiempo real desde Telegram (`/estado`, `/sensores`).
3. Enviar **alertas automáticas** cuando una variable supere su umbral, con
   *cooldown* para no saturar el chat.
4. Publicar telemetría MQTT cada 10 s en una plataforma IoT (Etapa 2).
5. Navegar de forma autónoma y reactiva, evitando obstáculos, con batería
   monitoreada (Etapa 3).

---

## 2. Fases de desarrollo

| Etapa | Alcance | Estado |
| :---: | --- | :---: |
| **1** | DHT22 + MQ-2/MQ-135 + flama IR, Wi-Fi, comandos y alertas Telegram | ✅ Implementada |
| **2** | Telemetría MQTT cada 10 s + dashboard (Ubidots / Blynk / ThingSpeak) | ⏳ Pendiente |
| **3** | HC-SR04, divisor de batería, L298N, 2 motores DC, Bluetooth serial y evasión reactiva | 🟡 Motores y Bluetooth serial implementados; percepción y evasión pendientes |

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
- Control manual desde el celular por Bluetooth serial.
- Paro automático si no llega una nueva orden de movimiento en **5 s**.

---

## 3. Arquitectura del firmware

```text
   Celular ──Bluetooth──► HC-05/06/HM-10 ──UART──► ESP32 GPIO16/17
                                                      │
                                                      ▼
                                                comandos.cpp
                                                      │
                                                      ▼
   DHT22  ───────► GPIO 13 ┐                    motores.cpp ──► L298N ──► motores
   MQ-2   ───────► GPIO 34 ├──► sensores.cpp ──┐
   Flama  ───────► GPIO 35 ┘                    │
                                                ▼
                                         alertas.cpp
                                                │
                                                ▼
   Wi-Fi ◄── wifi_manager.cpp ◄──────── telegram_bot.cpp
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
| `motores.*` | Control no bloqueante del L298N, movimientos temporizados, control continuo y demo |
| `comandos.*` | Procesamiento compartido de comandos USB y Bluetooth serial |
| `bluetooth_serial.*` | Puente UART con el módulo HC-05/06/HM-10 |
| `secrets.h` | Credenciales reales (**ignorado por Git**) |
| `secrets.example.h` | Plantilla de credenciales |

### Regla de oro: código no bloqueante

Toda temporización se realiza con `millis()`. **No se usan `delay()` largos**.
Solo se usa `delay(1)` al final del `loop()` para ceder tiempo al Wi-Fi y evitar
el watchdog. Esto es indispensable para la Etapa 3: el lazo principal debe
seguir atendiendo el Bluetooth serial, el ultrasonido y los motores aunque se
esté esperando el siguiente muestreo.

> Nota técnica: las librerías `DHT` y `UniversalTelegramBot` realizan esperas
> internas breves (lectura del sensor y handshake TLS). Se aceptan en la Etapa 1
> y se medirán antes de integrar la navegación autónoma.

---

## 4. Pinout oficial (ESP32)

### 4.1 Etapa 1 — sensores ambientales

| Componente | Señal | GPIO | Alimentación | Notas |
| --- | --- | :---: | :---: | --- |
| **DHT22** | DATA | **13** | 3.3 V | El módulo ya incluye pull-up de 10 kΩ |
| **MQ-2 / MQ-135** | AO | **34** | 5 V (VIN) | ADC1, solo entrada. Línea base ≈ 2000 |
| **Flama IR (LM393)** | DO / OUT | **35** | 3.3 V | ADC1, solo entrada. `LOW` = fuego, `HIGH` = normal |

> ⚠️ **Protección del ADC:** el módulo MQ se alimenta a 5 V y su salida analógica
> puede acercarse a 5 V en concentraciones altas. En el montaje final es
> **obligatorio** el divisor de 20 kΩ / 10 kΩ antes de GPIO 34. Durante las
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
| **HC-05/06/HM-10** | TX | **16** | TX del módulo → RX2 de la ESP32 |
| **HC-05/06/HM-10** | RX | **17** | RX del módulo ← TX2 de la ESP32 |

Criterios del mapa de pines:

- Entradas analógicas en **ADC1** (GPIO 32–39) para que el Wi-Fi no interfiera.
- **GPIO 12 evitado** por ser pin de arranque (*strapping*).
- GPIO 34, 35 y 36 son **solo entrada**, sin pull-up interno.
- El HC-SR04 ocupa GPIO 32 (TRIG) y GPIO 33 (ECHO); por eso la batería se movió
  a **GPIO 36** y ENB a **GPIO 26**.
- El ECHO del HC-SR04 entrega 5 V: es **obligatorio** el divisor 1 kΩ / 2 kΩ
  antes de GPIO 33.
- Alimentación de motores **independiente** de la ESP32, con **GND común**.

---

## 5. Cableado completo para arrancar

Esta es la parte que suele olvidarse. Haz las conexiones **con la alimentación
desconectada** y revisa la polaridad antes de encender.

### 5.1 Batería → L298N → motores

| Terminal L298N | Conexión |
| --- | --- |
| **+12V / VMS** | Positivo del pack 2×18650 (7.4–8.4 V) |
| **GND** | Negativo de la batería **y** GND de la ESP32 |
| **+5V** | Salida de 5 V del L298N (con jumper puesto). **No** conectar al pin 3.3 V de la ESP32 |
| **OUT1 / OUT2** | Motor A (izquierdo o derecho, según tu montaje) |
| **OUT3 / OUT4** | Motor B (el otro motor) |
| **IN1** | GPIO 18 |
| **IN2** | GPIO 19 |
| **IN3** | GPIO 22 |
| **IN4** | GPIO 23 |
| **ENA / ENB** | Dejar los **jumpers puestos** para velocidad fija |

```text
Batería 2S (+) ──────────────► L298N +12V
Batería 2S (−) ──────────────► L298N GND ─────► ESP32 GND (común)
L298N OUT1/OUT2 ─────────────► Motor A
L298N OUT3/OUT4 ─────────────► Motor B
L298N IN1/IN2/IN3/IN4 ───────► GPIO 18/19/22/23
L298N +5V ───────────────────► ESP32 VIN (solo si usas su regulador)
```

Reglas importantes:

- El L298N **no se alimenta desde la ESP32**. La potencia de los motores va a la
  batería.
- El **GND es común**: batería (−), L298N GND, ESP32 GND, sensores GND y módulo
  Bluetooth GND deben estar unidos.
- **No** conectes el positivo de la batería al pin `3V3` ni al pin `5V` de la
  ESP32. La batería va al terminal `+12V` del L298N.
- Si un motor gira al revés, cambia `MOTOR_A_INVERTIDO` o `MOTOR_B_INVERTIDO` a
  `true` en `config.h`, o intercambia sus dos cables en `OUT1/OUT2` u
  `OUT3/OUT4`.

### 5.2 Alimentación de la ESP32

Tienes dos opciones:

**Opción A — rápida, con el regulador del L298N (2×18650):**

1. Deja puesto el jumper de 5 V del L298N.
2. Conecta **L298N +5V → ESP32 VIN**.
3. Une **L298N GND → ESP32 GND**.
4. Alimenta el módulo Bluetooth y los sensores de 5 V desde ese mismo `+5V`.
5. **Desconecta el USB** de la ESP32 cuando uses la batería.

**Opción B — recomendada para el montaje final:**

- Usa un **convertidor reductor 5 V** independiente (o una power bank) para la
  ESP32, y deja la batería solo para el L298N. Une todos los GND.
- Es más estable para los picos de corriente del Wi-Fi.

> ⚠️ **Durante la programación por USB:** no conectes al mismo tiempo el `+5V`
> del L298N al `VIN` de la ESP32. Alimenta la ESP32 por USB y deja el `+5V` del
> L298N desconectado. Solo mantén el **GND común**. Si necesitas alimentar el
> HC-05 mientras pruebas por USB, usa el pin `5V` de la ESP32.

> Si alimentas el L298N con **más de 12 V**, quita el jumper de 5 V y usa una
> fuente de 5 V externa. El regulador del L298N se calienta y no es la mejor
> opción para la ESP32 en el montaje definitivo.

### 5.3 Divisor de batería (GPIO 36)

Para medir la batería de 2×18650:

```text
Batería (+) ──[ 20 kΩ ]──┬── GPIO 36
                         │
                     [ 10 kΩ ]
                         │
Batería (−) ─────────────┴── GND
```

- Con 8.4 V, el punto medio entrega **2.8 V** al ADC. Es seguro para GPIO 36.
- GPIO 36 es **solo entrada**; no tiene pull-up interno.
- Si todavía no vas a usar la lectura de batería, puedes dejar este divisor
  desconectado; no afecta el movimiento.

### 5.4 Módulo Bluetooth serial

| Módulo HC-05 / HC-06 / HM-10 | ESP32 | Notas |
| --- | :---: | --- |
| **VCC** | **5V** | La mayoría de breakouts aceptan 5 V. Si el módulo es solo de 3.3 V, aliméntalo con 3.3 V |
| **GND** | **GND** | GND común obligatorio |
| **TX** | **GPIO 16** | TX del módulo → RX2 de la ESP32 |
| **RX** | **GPIO 17** | RX del módulo ← TX2 de la ESP32 |
| **STATE** | No conectado | Opcional; el firmware no lo usa |

```text
HC-05/06/HM-10 TX ───────────► GPIO 16 (RX2)
HC-05/06/HM-10 RX ◄─────────── GPIO 17 (TX2)
HC-05/06/HM-10 GND ─────────── GND común
HC-05/06/HM-10 VCC ─────────── 5 V (o 3.3 V según el módulo)
```

- **Cruza TX/RX**: el TX del módulo va al GPIO 16 y el RX del módulo al GPIO 17.
- El módulo HC-05 transmite a **3.3 V**; la ESP32 lo tolera. El TX de la ESP32
  también es de 3.3 V, así que el RX del módulo recibe un nivel seguro.
- **No** metas 5 V en el pin RX del módulo.
- El módulo debe ser un **puente serial** (HC-05, HC-06, HM-10). Un módulo de
  audio Bluetooth o un receptor USB no sirven para este firmware.
- Si tu módulo es **HM-10**, es BLE: se usa con una app BLE, no con la app
  clásica de HC-05. El código y el cableado UART son los mismos.
- La velocidad por defecto es **9600 baudios**. Si tu módulo viene configurado a
  38400 o 115200, cambia `BT_BAUDIOS` en `config.h`.
- El firmware ignora los mensajes de estado del módulo (`+READY`, `+CONNECTED`,
  etc.) para que no se confundan con comandos de movimiento.

### 5.5 Checklist rápido de conexiones olvidadas

- [ ] Batería (+) → L298N `+12V`.
- [ ] Batería (−) → L298N `GND`.
- [ ] L298N `GND` → ESP32 `GND` (GND común).
- [ ] Motor A → L298N `OUT1/OUT2`.
- [ ] Motor B → L298N `OUT3/OUT4`.
- [ ] L298N `IN1–IN4` → GPIO 18/19/22/23.
- [ ] Jumpers `ENA` y `ENB` puestos.
- [ ] HC-05 `TX` → GPIO 16, `RX` → GPIO 17, `GND` común.
- [ ] HC-05 `VCC` → 5 V (o 3.3 V si el módulo es solo de 3.3 V).
- [ ] Divisor de batería: 20 kΩ a GPIO 36 y 10 kΩ a GND (opcional por ahora).
- [ ] La ESP32 **no** está alimentada directamente por la batería.

---

## 6. Prueba de motores y Bluetooth

### 6.1 Preparación

1. Coloca el rover **sobre bloques** para que las ruedas giren sin desplazarse.
2. Verifica que los jumpers **ENA** y **ENB** estén puestos.
3. Conecta la alimentación del L298N y une el GND con la ESP32.
4. Empareja el celular con el módulo Bluetooth (ver sección 6.2).
5. Abre el Monitor Serial USB a **115200** baudios.

### 6.2 Emparejar el celular

Para **HC-05 / HC-06**:

1. Alimenta el módulo Bluetooth.
2. En el celular: **Ajustes → Bluetooth → buscar dispositivos**.
3. Selecciona `HC-05` o `HC-06`.
4. PIN habitual: **1234** o **0000**.
5. Instala una app de terminal Bluetooth. En Android funciona bien
   **Serial Bluetooth Terminal**.
6. Conéctate al módulo desde la app.

Para **HM-10 (BLE)**:

1. Usa una app BLE, por ejemplo **nRF Connect** o **BLE Terminal**.
2. Busca el módulo y conéctate.
3. Envía las letras por la característica de escritura.

> **iPhone:** el protocolo clásico SPP de HC-05/HC-06 no está disponible. Usa un
> Android o un módulo BLE HM-10 con una app BLE.

### 6.3 Comandos por USB o Bluetooth

Envía **una sola letra** por la app o por el Monitor Serial. La app puede agregar
Enter; el firmware lo ignora.

| Tecla | Acción |
| :---: | --- |
| `a` | Adelante (continuo) |
| `r` | Atrás (continuo) |
| `i` | Girar a la izquierda (continuo) |
| `d` | Girar a la derecha (continuo) |
| `s` | Detener inmediatamente |
| `p` | Demo: adelante, atrás, izquierda, derecha |
| `m` | Mostrar el estado de los motores |
| `h` o `?` | Ayuda |

- `a/r/i/d` mantienen el movimiento hasta que envíes `s` **o** hasta que pasen
  **5 segundos sin una nueva orden**. Cada letra refresca el temporizador.
- Para recorrer una distancia mayor, envía `a` de nuevo antes de que se cumplan
  los 5 s.
- `s` es el **paro de emergencia**.
- Si el enlace Bluetooth se corta, el rover se detiene al vencer los 5 s.
- También se aceptan palabras completas: `adelante`, `atras`, `izquierda`,
  `derecha`, `detener`, `demo`, `estado`, `ayuda`.
- Puedes enviar la letra con o sin Enter. Si la app no agrega Enter, el firmware
  la procesa tras un silencio muy breve.

### 6.4 Comandos por Telegram

Los comandos de Telegram son independientes del Bluetooth y duran **1.5 s**:

- `/demo` — secuencia completa.
- `/adelante`, `/atras`, `/izquierda`, `/derecha`.
- `/detener` — paro inmediato.

### 6.5 Si un motor no se mueve

1. Revisa que el jumper `ENA` o `ENB` correspondiente esté puesto.
2. Verifica el GND común entre batería, L298N y ESP32.
3. Comprueba que la batería tenga carga y que el interruptor esté encendido.
4. Prueba `/demo` desde Telegram o `p` por USB.
5. Si solo gira un motor, revisa los cables `OUT1/OUT2` u `OUT3/OUT4` y las
   señales `IN1–IN4`.
6. Si gira al revés, invierte los dos cables de ese motor o cambia la bandera
   `MOTOR_A_INVERTIDO` / `MOTOR_B_INVERTIDO` en `config.h`.

---

## 7. Umbrales y calibración

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

> ⚠️ **Importante para la Etapa 3:** al instalar el divisor de 20 kΩ / 10 kΩ, la
> tensión que llega al ADC se reduce a **2/3**. La línea base de ~2000 bajará a
> ~1333 y el umbral de 2700 deberá recalibrarse (referencia inicial: ~1800).
> El valor **2700 corresponde a la conexión directa actual**, sin divisor.

La lectura de gas mostrada y evaluada es el **promedio de 8 muestras ADC**; el
valor crudo de la última muestra se incluye en el Monitor Serial.

---

## 8. Bot de Telegram

### Comandos

| Comando | Acción |
| --- | --- |
| `/start` | Bienvenida y ayuda |
| `/estado` | Lecturas actuales, estado de motores, Bluetooth, Wi-Fi y uptime |
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
| `/bluetooth` | Instrucciones para conectar el celular por Bluetooth serial |
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

## 9. Estructura del repositorio

```text
rbtmvll/
├── rbtmvll.ino             # Sketch principal (setup / loop)
├── config.h                # Pines, umbrales y tiempos
├── sensores.h / .cpp       # DHT22, MQ-2/MQ-135 y flama IR
├── wifi_manager.h / .cpp   # Wi-Fi no bloqueante y reconexión
├── telegram_bot.h / .cpp   # Bot, comandos y respuestas
├── alertas.h / .cpp        # Umbrales, cooldown y normalización
├── motores.h / .cpp        # L298N, movimientos, control continuo y demo
├── comandos.h / .cpp       # Comandos compartidos USB / Bluetooth
├── bluetooth_serial.h/.cpp # Puente UART con HC-05/06/HM-10
├── secrets.example.h       # Plantilla de credenciales (sí se versiona)
├── secrets.h               # Credenciales reales (IGNORADO por Git)
├── .gitignore
└── README.md
```

> Arduino IDE compila todos los `.cpp`/`.h` que estén en la misma carpeta del
> sketch. Mantener esa estructura para conservar la compatibilidad.

---

## 10. Puesta en marcha

### 10.1 Requisitos de software

- Arduino IDE 2.x.
- Núcleo **ESP32 by Espressif Systems** (`3.3.12` o superior).
- Bibliotecas (Library Manager):
  - **DHT sensor library** de Adafruit (`1.4.7` o superior).
  - **Adafruit Unified Sensor** (dependencia de la anterior).
  - **UniversalTelegramBot** de Brian Lough (`1.3.0` o superior).
  - **ArduinoJson** (`7.x`).

> Ya **no** se necesita instalar el paquete **ESP32 + Bluepad32**. El proyecto
> usa el core estándar de ESP32 y un módulo Bluetooth externo.

### 10.2 Configurar credenciales

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

### 10.3 Compilar y cargar

Desde Arduino IDE:

1. Abrir `rbtmvll.ino`.
2. Seleccionar la placa **ESP32 Dev Module** del paquete **ESP32 by Espressif
   Systems**.
3. Esquema de partición: **Default** (no se necesita Huge APP).
4. **Verificar** y luego **Subir**.
5. Abrir el Monitor Serial a **115200 baudios**.

Desde Arduino CLI:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 .
arduino-cli upload -p COM9 --fqbn esp32:esp32:esp32 .
```

> Si el puerto está ocupado, cierra el Monitor Serial de Arduino IDE antes de
> subir. El puerto detectado en este equipo es **COM9**; cámbialo si aparece
> otro.

### 10.4 Salida esperada del Monitor Serial

```text
==============================================
 IndoorWatch - Rover IoT de monitoreo
 Etapa 1 + motores L298N + Bluetooth serial
==============================================
[SENSORES] DHT22=GPIO13, MQ=GPIO34, FLAMA=GPIO35
[MOTORES] L298N listo: IN1=18 IN2=19 IN3=22 IN4=23. ENA/ENB con jumpers (velocidad fija).
[MOTORES] USB/Bluetooth: a=adelante r=atras i=izquierda d=derecha s=detener p=demo
[BT] Modulo serial listo: RX=GPIO16 TX=GPIO17 @ 9600 baudios.
[BT] Empareja el celular y envia a/r/i/d/s/p.
[BT] Ayuda: h | estado: m
[WIFI] Conectando a la red: MiRed
[SISTEMA] Listo. USB 115200 | Bluetooth serial 9600 baudios.
[WIFI] Conectado. IP: 192.168.1.50 | RSSI: -62 dBm
[TELEGRAM] Aviso de inicio enviado correctamente.
[SENSOR] T=25.4 C | H=58.7 % | GAS=1987 (crudo=2003) | FLAMA=NO
```

---

## 11. Verificación funcional (checklist)

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
- [ ] Los comandos `a`, `r`, `i`, `d`, `s`, `p`, `m` y `h` funcionan por USB.
- [ ] El módulo Bluetooth aparece en el celular y se empareja.
- [ ] Al enviar `a` desde la app, ambos motores avanzan.
- [ ] Al enviar `s`, los motores se detienen.
- [ ] `i` y `d` giran; `r` retrocede.
- [ ] Si no se envía otra orden en 5 s, el rover se detiene solo.
- [ ] `p` ejecuta la demo y `m` muestra el estado.

> Para probar alertas sin fuego real: usar una fuente de luz IR o un
> encendedor **sin encender** para gas; nunca exponer los sensores a llamas
> abiertas ni a humo dentro de una habitación cerrada.

---

## 12. Seguridad

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
- No conectar el positivo de la batería directamente a la ESP32. La batería va
  al L298N.
- El módulo MQ necesita el divisor de tensión definitivo antes de conectar su
  salida al ADC de la ESP32.
- Antes de probar los motores, colocar el rover sobre bloques. Los comandos
  `/demo`, `/adelante`, `/atras`, `/izquierda`, `/derecha` y las letras
  `a/r/i/d` pueden moverlo.
- Los jumpers ENA/ENB deben estar puestos si no se usa PWM; de lo contrario los
  motores no recibirán habilitación.
- Mantener una mano cerca del interruptor de alimentación. `s` es el paro de
  emergencia por Bluetooth; `/detener` es el paro por Telegram.
- El control continuo se detiene automáticamente a los **5 s** sin una nueva
  orden. Si se pierde el enlace Bluetooth, el rover no sigue indefinidamente.
- El módulo HC-05/06 usa un PIN de emparejamiento débil (1234/0000). No usar el
  rover en un entorno público sin cambiar el PIN o añadir autenticación.
- **HC-05/06/HM-10 no pueden actuar como host HID del mando GameSir.** Si en el
  futuro se quiere volver a usar el mando directo, habría que recuperar
  Bluepad32 o usar un host USB; no es el alcance actual.

---

## 13. Flujo de trabajo Git

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

## 14. Próximos pasos

1. Validar en hardware los motores y el módulo Bluetooth con el rover sobre
   bloques; ajustar los sentidos de giro.
2. Confirmar la alimentación definitiva: batería → L298N, 5 V para la ESP32 y
   GND común; medir tensiones antes de conectar los sensores.
3. Integrar el HC-SR04 y el divisor de batería; validar el ECHO con su divisor.
4. Implementar la máquina de estados de evasión: `AVANZAR → DETENER →
   RETROCEDER → GIRAR → AVANZAR`.
5. Agregar el módulo MQTT (Etapa 2) manteniendo la arquitectura no bloqueante:
   publicación cada 10 s a `rover/temp`, `rover/hum`, `rover/gas`,
   `rover/flama` y `rover/bateria`.
6. Añadir PWM en ENA/ENB para velocidad proporcional y control más fino.
7. Añadir pruebas de integración y registro de datos para el dashboard.

---

<p align="center">Proyecto académico de sistemas embebidos, IoT y robótica · ESP32</p>
