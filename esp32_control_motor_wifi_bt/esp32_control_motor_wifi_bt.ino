/*
 * Control de giro con sensor - ESP32 DevKit 30 pines  (WiFi + Bluetooth)
 *
 * Dos formas de control, el ESP32 elige solo:
 *
 *   - Si hay un telefono conectado por BLUETOOTH ("ESP32_Giro"): manda el telefono.
 *     El ESP32 decide el modo automatico localmente con el sensor.
 *   - Si NO hay telefono por Bluetooth: manda la API por WiFi (Cloudflare Worker),
 *     igual que antes (POST /api/distance + GET /api/status).
 *
 * Protocolo Bluetooth (texto, una linea terminada en \n):
 *   App -> ESP32 :  LEFT | RIGHT | STOP | AUTO | MANUAL
 *   ESP32 -> App :  <cm>;<modo>;<motor>      ejemplo:  152;auto;right
 *
 * Modo automatico (en Bluetooth): 100-200 cm derecha, 200-300 cm izquierda, fuera se detiene.
 * (En WiFi esos rangos los calcula la API.)
 *
 * Libreria necesaria: ArduinoJson v7. Placa: "ESP32 Dev Module".
 * IMPORTANTE: Herramientas -> Partition Scheme -> "Huge APP (3MB No OTA/1MB SPIFFS)"
 *             (WiFi + HTTPS + Bluetooth no caben en la particion por defecto).
 */
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>

// ------------------- -------------------
const char* WIFI_SSID = "Adriana";       // red de 2.4 GHz
const char* WIFI_PASS = "aaa12356";
const char* API_BASE  = "https://mi-control-giro-api.girogiro.workers.dev";
const char* BT_NOMBRE = "ESP32_Giro";          // nombre que veras al emparejar el telefono

const bool USAR_SENSOR = true;    // true = sensor HC-SR04 conectado (false para probar sin sensor)
const unsigned long INTERVALO_RED_MS = 700;   // cada cuanto consulta a la API
// -------------------------------------------------------

// Pines
const int TRIG_PIN = 18;
const int ECHO_PIN = 34;
const int ENA_PIN  = 14;
const int IN1_PIN  = 26;
const int IN2_PIN  = 27;

String modo = "manual";
String estadoMotor = "detener";
float distancia = -1;            // -1 = sin lectura
int fallosApi = 0;
unsigned long tSensor = 0, tRed = 0, tBt = 0;

// Estado del control por Bluetooth
BluetoothSerial SerialBT;
bool btAuto = false;
String btMotor = "stop";         // right / left / stop
String btBuffer = "";
bool btEraActivo = false;

WiFiClientSecure cliente;
HTTPClient http;   // global para reutilizar la conexion HTTPS entre peticiones

void aplicarMotor(const String& cmd) {
  if (cmd == "derecha") {
    digitalWrite(IN1_PIN, HIGH); digitalWrite(IN2_PIN, LOW);
    estadoMotor = "derecha";
  } else if (cmd == "izquierda") {
    digitalWrite(IN1_PIN, LOW);  digitalWrite(IN2_PIN, HIGH);
    estadoMotor = "izquierda";
  } else {
    digitalWrite(IN1_PIN, LOW);  digitalWrite(IN2_PIN, LOW);
    estadoMotor = "detener";
  }
}

void aplicarMotorIngles(const String& m) {
  if (m == "right")      aplicarMotor("derecha");
  else if (m == "left")  aplicarMotor("izquierda");
  else                   aplicarMotor("detener");
}

float leerDistancia() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  unsigned long dur = pulseIn(ECHO_PIN, HIGH, 30000);  // maximo 30 ms
  if (dur == 0) return -1;
  return dur * 0.0343 / 2.0;
}

// Rangos del modo automatico por Bluetooth
String motorAuto(float d) {
  if (d >= 100 && d <= 200) return "right";
  if (d > 200 && d <= 300)  return "left";
  return "stop";                       // fuera de rango o sin lectura
}

void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Conectando a WiFi");
  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 15000) { delay(500); Serial.print("."); }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("Sin WiFi por ahora (Bluetooth sigue disponible)");
  }
}

void falloApi() {
  if (++fallosApi >= 5) aplicarMotor("detener");   // seguridad: sin API, motor parado
}

void sincronizarConApi() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    falloApi();
    return;
  }

  int code;

  // 1) Enviar la distancia medida (solo si el sensor esta conectado)
  if (USAR_SENSOR) {
    // sin eco o fuera de rango se envia 400 (la API acepta de 0 a 400)
    int cm = (distancia < 0 || distancia > 400) ? 400 : (int)round(distancia);
    http.begin(cliente, String(API_BASE) + "/api/distance");
    http.addHeader("Content-Type", "application/json");
    code = http.POST("{\"distance\":" + String(cm) + "}");
    if (code <= 0 || code >= 300) Serial.printf("POST /api/distance fallo: %d\n", code);
    http.end();
  }

  // 2) Leer el estado y mover el motor como indique la API
  http.begin(cliente, String(API_BASE) + "/api/status");
  code = http.GET();
  if (code == 200) {
    JsonDocument doc;
    if (!deserializeJson(doc, http.getString())) {
      modo = String(doc["mode"] | "manual");
      aplicarMotorIngles(String(doc["motor"] | "stop"));
      fallosApi = 0;
    }
  } else {
    Serial.printf("GET /api/status fallo: %d\n", code);
    falloApi();
  }
  http.end();

  Serial.printf("[WiFi] modo=%s motor=%s distancia=%.1f\n", modo.c_str(), estadoMotor.c_str(), distancia);
}

// ---------------- Bluetooth ----------------
void procesarComandoBT(String cmd) {
  cmd.trim();
  cmd.toUpperCase();
  if (cmd == "AUTO") {
    btAuto = true;
  } else if (cmd == "MANUAL") {
    btAuto = false;
    btMotor = "stop";
  } else if (!btAuto) {                 // en automatico se ignoran los botones
    if (cmd == "RIGHT")      btMotor = "right";
    else if (cmd == "LEFT")  btMotor = "left";
    else if (cmd == "STOP")  btMotor = "stop";
  }
}

void leerBluetooth() {
  while (SerialBT.available()) {
    char c = (char)SerialBT.read();
    if (c == '\n' || c == '\r') {
      if (btBuffer.length() > 0) procesarComandoBT(btBuffer);
      btBuffer = "";
    } else if (btBuffer.length() < 32) {
      btBuffer += c;
    }
  }
}

void enviarTelemetriaBT() {
  int cm = (distancia < 0 || distancia > 400) ? 400 : (int)round(distancia);
  SerialBT.printf("%d;%s;%s\n", cm, btAuto ? "auto" : "manual", btMotor.c_str());
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(ENA_PIN, OUTPUT);
  pinMode(IN1_PIN, OUTPUT);
  pinMode(IN2_PIN, OUTPUT);
  digitalWrite(ENA_PIN, HIGH);   // velocidad completa
  aplicarMotor("detener");

  SerialBT.begin(BT_NOMBRE);     // Bluetooth primero, para que funcione aunque no haya WiFi
  Serial.printf("Bluetooth listo: busca \"%s\" en el telefono\n", BT_NOMBRE);

  conectarWiFi();
  cliente.setInsecure();         // no valida el certificado; suficiente para el proyecto
  http.setReuse(true);
  http.setTimeout(5000);
}

void loop() {
  unsigned long ahora = millis();

  if (USAR_SENSOR && ahora - tSensor >= 150) {
    tSensor = ahora;
    distancia = leerDistancia();
  }

  // Cambio de fuente de control: al conectar/desconectar el telefono se reinicia a "detenido"
  bool btActivo = SerialBT.hasClient();
  if (btActivo != btEraActivo) {
    btEraActivo = btActivo;
    btAuto = false;
    btMotor = "stop";
    fallosApi = 0;
    aplicarMotor("detener");
    Serial.println(btActivo ? "Bluetooth conectado: manda el telefono"
                            : "Bluetooth desconectado: manda la API (WiFi)");
  }

  if (btActivo) {
    leerBluetooth();
    if (btAuto) btMotor = motorAuto(distancia);
    aplicarMotorIngles(btMotor);
    if (ahora - tBt >= 300) {
      tBt = ahora;
      enviarTelemetriaBT();
    }
  } else if (ahora - tRed >= INTERVALO_RED_MS) {
    tRed = ahora;
    sincronizarConApi();
  }
}
