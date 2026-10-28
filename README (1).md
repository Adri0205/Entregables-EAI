# Desafío Práctico Final – Control de giro con app móvil (EAI441)

**Facultad de Ingeniería · Escuela de Ingeniería en Computación**
**Asignatura:** EAI441 · **Semana:** 18 · **Grupo de laboratorio:** 01_

## Integrantes

| Nombre | Carnet |
|---|---|
| Adriana Eunice Flores Hernández | FH190090 |
| jose Ricardo Mendez Zelaya | MZ221613 |

## Descripción

Aplicación móvil Android (Kotlin + Jetpack Compose) que controla un motor DC conectado a un ESP32 y muestra en tiempo real las lecturas de un sensor ultrasónico HC-SR04. La app se comunica con el ESP32 de **dos formas**: por **WiFi** (a través de una API en Cloudflare Workers) y por **Bluetooth** (directo al ESP32).

### Funciones de la app
- Botones para manipular el motor: **izquierda, detener y derecha**.
- Lector/mostrador de la distancia del sensor ultrasónico.
- Botón/interruptor que **cede el mando del motor al sensor** (modo automático).
- Escudo del Barcelona en la pantalla principal.
- Selector de conexión: **WiFi** o **Bluetooth**.

### Modo automático (sensor controla el motor)
| Distancia del objeto | Motor |
|---|---|
| 100 cm – 200 cm | Gira a la derecha |
| 200 cm – 300 cm | Gira a la izquierda |
| Fuera de esos intervalos | Se detiene |

## Contenido de la entrega

```
Sketches/esp32_control_motor_wifi_bt/   Sketch del ESP32 (.ino)
App/ControlGiro.apk                     APK para instalar en el celular
App/codigo-fuente/                      Proyecto de Android Studio
API/                                    API (index.js + wrangler.toml)
```

## Hardware

- ESP32 DevKit (30 pines)
- Sensor ultrasónico HC-SR04 (el pin ECHO pasa por un divisor de voltaje)
- Driver L293D y motor DC (alimentado con fuente de 5 V; GND común con el ESP32)

**Pines usados en el ESP32:** TRIG 18 · ECHO 34 · ENA 14 · IN1 26 · IN2 27

## Cómo funciona

El ESP32 elige quién lo controla:

- **Con un celular conectado por Bluetooth** (`ESP32_Giro`): manda el celular. El ESP32 calcula localmente el modo automático con el sensor.
- **Sin Bluetooth conectado:** manda la API por WiFi. El ESP32 envía la distancia (`POST /api/distance`) y lee el estado (`GET /api/status`) para mover el motor.

Al conectar o desconectar el Bluetooth, el motor vuelve a detenido por seguridad. Si la API deja de responder, el motor también se detiene.

### API (WiFi)
URL: `https://mi-control-giro-api.girogiro.workers.dev`

| Ruta | Método | Función |
|---|---|---|
| `/api/status` | GET | Devuelve `{distance, mode, motor}` |
| `/api/distance` | POST | El ESP32 envía la distancia medida |
| `/api/mode` | POST | Cambia el modo (`manual` / `auto`) |
| `/api/motor` | POST | Mueve el motor en manual (`left`, `right`, `stop`) |

### Protocolo Bluetooth
- App → ESP32: `LEFT`, `RIGHT`, `STOP`, `AUTO`, `MANUAL` (una línea terminada en `\n`)
- ESP32 → App: `<cm>;<modo>;<motor>`, por ejemplo `152;auto;right`

## Instalación y uso

### 1. Sketch del ESP32
1. Abrir el `.ino` en Arduino IDE 2.x. Instalar la librería **ArduinoJson v7** y el core de **ESP32**.
2. Editar al inicio del archivo `WIFI_SSID` y `WIFI_PASS` (red de 2.4 GHz).
3. Herramientas → Placa: **ESP32 Dev Module**.
4. Herramientas → Partition Scheme: **Huge APP (3MB No OTA/1MB SPIFFS)**.
5. Subir el sketch con el botón Upload.

### 2. App móvil
1. Copiar `ControlGiro.apk` al celular y abrirlo (permitir instalar de orígenes desconocidos).
2. Para **Bluetooth**: emparejar antes el celular con `ESP32_Giro` en Ajustes → Bluetooth (si pide PIN: 1234 o 0000). En la app, elegir el modo Bluetooth, tocar "Elegir dispositivo" y seleccionar `ESP32_Giro`. Aceptar el permiso de dispositivos cercanos.
3. Para **WiFi**: elegir el modo WiFi (el celular y el ESP32 necesitan internet).
4. Usar los botones del motor, o activar el interruptor para que el sensor tome el mando.

## Tecnologías

Kotlin · Jetpack Compose · Material 3 · Bluetooth clásico (SPP) · Arduino C++ (ESP32) · Cloudflare Workers (Durable Objects)
