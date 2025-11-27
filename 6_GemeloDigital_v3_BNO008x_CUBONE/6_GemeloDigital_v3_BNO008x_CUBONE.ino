// ===========================
//   LIBRERÍAS
// ===========================
#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>

#include "SparkFun_BNO08x_Arduino_Library.h"
BNO08x myIMU;


// ===========================
//   PINES I2C ESP32-C3
// ===========================
#define SDA_PIN 5
#define SCL_PIN 6


// ===========================
//   OLED
// ===========================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN
);

const int xOffset = 27;
const int yOffset = 20;


// ===========================
//   LED AZUL
// ===========================
#define LED_PIN 8


// ===========================
//   WIFI
// ===========================
const char* ssid = "Mi_casa1";
const char* password = "ramses123";

AsyncWebServer server(80);
AsyncEventSource events("/events");


// ===========================
//   CONTROL ENVÍO SENSOR
// ===========================
unsigned long previousSend = 0;
const unsigned long sendInterval = 50; // 20Hz


// ===========================
//   ESTADO WIFI
// ===========================
unsigned long lastWifiCheck = 0;
bool wifiPreviouslyConnected = false;


// ===========================
//   OLED — PANTALLAS
// ===========================
void drawConnecting(uint8_t dots)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  u8g2.setCursor(xOffset + 2, yOffset + 10);
  u8g2.print("Conectando");

  u8g2.setCursor(xOffset + 2, yOffset + 22);
  for(int i=0; i<dots; i++)
    u8g2.print(".");

  u8g2.sendBuffer();
}

void drawIP()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  u8g2.setCursor(xOffset + 5, yOffset + 10);
  u8g2.print("WiFi OK");

  u8g2.setCursor(xOffset - 0.3, yOffset + 22);
  u8g2.print(WiFi.localIP().toString());

  u8g2.sendBuffer();
}

void drawDisconnected()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  u8g2.setCursor(xOffset - 10, yOffset + 15);
  u8g2.print("WiFi perdido");

  u8g2.sendBuffer();
}


// ===========================
//   WIFI — CONEXIÓN INICIAL
// ===========================
void connectWiFiInitial() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint8_t dots = 0;

  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    drawConnecting(dots);
    dots = (dots + 1) % 4;
    delay(400);
  }

  digitalWrite(LED_PIN, LOW);

  Serial.println("✅ WiFi conectado");
  Serial.println(WiFi.localIP());
  drawIP();
  wifiPreviouslyConnected = true;
}


// ===========================
//   WIFI — RECONEXIÓN
// ===========================
void checkWiFi() {
  unsigned long now = millis();
  if (now - lastWifiCheck < 3000) return;
  lastWifiCheck = now;

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiPreviouslyConnected) {
      Serial.println("⚠️ WiFi desconectado");
      drawDisconnected();
      digitalWrite(LED_PIN, HIGH);
      wifiPreviouslyConnected = false;
    }
    WiFi.reconnect();
  } 
  else if (!wifiPreviouslyConnected) {
    Serial.println("✅ WiFi reconectado");
    Serial.println(WiFi.localIP());
    drawIP();
    digitalWrite(LED_PIN, LOW);
    wifiPreviouslyConnected = true;
  }
}


// ===========================
//   SETUP
// ===========================
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  u8g2.begin();
  u8g2.setContrast(255);

  Wire.begin(SDA_PIN, SCL_PIN, 400000);

  // ---- BNO08X ----
  Serial.println("Iniciando IMU...");
  if (!myIMU.begin()) {
    Serial.println("❌ ERROR IMU no detectada");
    while (1) {}
  }

  Serial.println("✅ IMU detectada");
  
  // ✅ MENOS DRIFT
  myIMU.enableGameRotationVector(10);

  // ---- WIFI ----
  connectWiFiInitial();

  // ---- LittleFS ----
  if (!LittleFS.begin(true)) {
    Serial.println("❌ ERROR LittleFS");
    while (1) {}
  }

  // ---- SERVIDOR WEB ----
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // ---- SSE ----
  events.onConnect([](AsyncEventSourceClient* client) {
    Serial.println("🌐 Cliente SSE conectado");
  });

  server.addHandler(&events);
  server.begin();

  Serial.println("✅ Servidor listo");
  Serial.print("🌐 Abre: http://");
  Serial.println(WiFi.localIP());
}


// ===========================
//   LOOP PRINCIPAL
// ===========================
void loop() {
  checkWiFi();

  unsigned long now = millis();

  if (WiFi.status() == WL_CONNECTED && now - previousSend >= sendInterval) {
    previousSend = now;

    if (myIMU.getSensorEvent()) {

      // ✅ GAME ROTATION VECTOR SIN DRIFT
      if (myIMU.sensorValue.sensorId == SH2_GAME_ROTATION_VECTOR) {

        float q_i = myIMU.sensorValue.un.gameRotationVector.i;
        float q_j = myIMU.sensorValue.un.gameRotationVector.j;
        float q_k = myIMU.sensorValue.un.gameRotationVector.k;
        float q_r = myIMU.sensorValue.un.gameRotationVector.real;

        StaticJsonDocument<128> doc;
        doc["x"] = q_i;
        doc["y"] = q_j;
        doc["z"] = q_k;
        doc["w"] = q_r;

        char buffer[128];
        serializeJson(doc, buffer);

        // ✅ ENVÍO SSE
        events.send(buffer, "quat", millis());
      }
    }
  }
}
