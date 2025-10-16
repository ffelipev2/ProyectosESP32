//https://github.com/mathieucarbou/ESPAsyncWebServer
//https://github.com/mathieucarbou/AsyncTCP
#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// ===== Pines I2C ESP32-S3 Zero =====
#define SDA_PIN 5
#define SCL_PIN 4

// ===== LED RGB onboard (WS2812) =====
#ifndef RGB_PIN
#define RGB_PIN 21   // Waveshare ESP32-S3 Zero -> WS2812 en GPIO21
#endif
extern "C" void neopixelWrite(uint8_t pin, uint8_t red, uint8_t green, uint8_t blue);
static inline void setLED(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_PIN, r, g, b);
}

// ===== WiFi =====
const char* ssid = "Mi_casa";
const char* password = "ramses123";

// ===== Servidor Web y SSE =====
AsyncWebServer server(80);
AsyncEventSource events("/events");

// ===== BNO055 =====
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29);

// ===== Control de envío =====
unsigned long previousSend = 0;
const unsigned long sendInterval = 50;  // 20 Hz

// ===== WiFi Control =====
unsigned long lastWifiCheck = 0;
const unsigned long wifiCheckInterval = 5000; // cada 5 segundos
bool wifiPreviouslyConnected = false;
unsigned long wifiConnectedTime = 0;
bool ledOffAfterDelay = false;

// ---------- FUNCIONES WIFI ----------

// Conexión inicial WiFi (bloqueante solo en setup)
void connectWiFiInitial() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    setLED(0, 0, 64);  // azul on
    delay(200);
    setLED(0, 0, 0);   // off
    delay(200);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  setLED(0, 64, 0);  // Verde
  wifiConnectedTime = millis();
  ledOffAfterDelay = false;
  wifiPreviouslyConnected = true;
}

// Reconexión no bloqueante
void checkWiFi() {
  unsigned long now = millis();
  if (now - lastWifiCheck < wifiCheckInterval) return;
  lastWifiCheck = now;

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiPreviouslyConnected) {
      Serial.println("WiFi desconectado. Intentando reconectar...");
      wifiPreviouslyConnected = false;
      setLED(0, 0, 64); // azul
    }
    WiFi.reconnect();
  } else if (!wifiPreviouslyConnected) {
    Serial.println("WiFi reconectado.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    setLED(0, 64, 0); // verde
    wifiConnectedTime = now;
    ledOffAfterDelay = false;
    wifiPreviouslyConnected = true;
  }
}

// ---------- SETUP ----------
void setup() {
  Serial.begin(115200);
  delay(300);
  setLED(0, 0, 0);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // BNO055
  Serial.println("Iniciando BNO055...");
  if (!bno.begin()) {
    Serial.println("No se pudo encontrar el BNO055.");
    while (1) { setLED(64, 0, 0); delay(200); setLED(0, 0, 0); delay(200); }
  }
  bno.setExtCrystalUse(true);
  Serial.println("BNO055 listo.");

  // WiFi
  connectWiFiInitial();

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Error al montar LittleFS.");
    while (1) { setLED(64, 0, 0); delay(300); setLED(0, 0, 0); delay(200); }
  }
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // SSE
  events.onConnect([](AsyncEventSourceClient* client) {
    Serial.println("Cliente conectado SSE");
  });
  server.addHandler(&events);

  server.begin();
}

// ---------- LOOP ----------
void loop() {
  checkWiFi();

  unsigned long now = millis();

  // Apaga LED 10 segundos después de conectarse
  if (!ledOffAfterDelay && WiFi.status() == WL_CONNECTED && (now - wifiConnectedTime > 10000)) {
    setLED(0, 0, 0);  // apaga LED
    ledOffAfterDelay = true;
  }

  // Envía datos del sensor
  if (WiFi.status() == WL_CONNECTED && now - previousSend >= sendInterval) {
    previousSend = now;

    imu::Quaternion quat = bno.getQuat();

    // JSON seguro y eficiente
    StaticJsonDocument<128> doc;
    doc["x"] = quat.x();
    doc["y"] = quat.y();
    doc["z"] = quat.z();
    doc["w"] = quat.w();

    char buffer[128];
    size_t len = serializeJson(doc, buffer);
    events.send(buffer, "quat", millis());
  }
}
