// ===========================================================
// LIBRERÍAS
// ===========================================================
#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <U8g2lib.h>

#include "SparkFun_BNO08x_Arduino_Library.h"
BNO08x myIMU;

// ===========================================================
// PINES ESP32-C3
// ===========================================================
#define SDA_PIN 5
#define SCL_PIN 6

// ===========================================================
// OLED CONFIG (MISMO ESTILO QUE TU CÓDIGO ORIGINAL)
// ===========================================================
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(
  U8G2_R0, U8X8_PIN_NONE, SCL_PIN, SDA_PIN
);

const int xOffset = 27;
const int yOffset = 20;

// ===========================================================
// WIFI + SSE
// ===========================================================
const char* ssid = "Mi_casa";
const char* password = "ramses123";

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long previousSend = 0;
const unsigned long sendInterval = 25;

// ===========================================================
// ESTADOS WIFI
// ===========================================================
bool wifiPreviouslyConnected = false;
unsigned long wifiConnectedTime = 0;
unsigned long lastWifiCheck = 0;

// ===========================================================
// MODO CALIBRACIÓN
// ===========================================================
bool calibrationMode = false;

// ===========================================================
// OLED FUNCTIONS (MISMO ESTILO QUE TU PRIMER PROYECTO)
// ===========================================================
void drawConnecting(uint8_t dots)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(xOffset+2, yOffset + 10);
  u8g2.print("Conectando");
  u8g2.setCursor(xOffset+2, yOffset + 22);
  for(int i=0; i<dots; i++) u8g2.print(".");
  u8g2.sendBuffer();
}

void drawIP()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(xOffset +5 , yOffset + 10);
  u8g2.print("WiFi OK");
  u8g2.setCursor(xOffset-0.1, yOffset + 22);
  u8g2.print(WiFi.localIP().toString());
  u8g2.sendBuffer();
}

void drawCalibration()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(xOffset+5, yOffset + 10);
  u8g2.print("CALIBRANDO");
  u8g2.sendBuffer();
}

void drawError(const char* msg)
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);
  u8g2.setCursor(xOffset + 5, yOffset + 10);
  u8g2.print("ERROR");
  u8g2.setCursor(xOffset + 5, yOffset + 22);
  u8g2.print(msg);
  u8g2.sendBuffer();
}

// ===========================================================
// CALIBRACIÓN
// ===========================================================
void startCalibrationMode() {
  calibrationMode = true;
  Serial.println("=== MODO CALIBRACIÓN ACTIVADO ===");
  drawCalibration();
}

void stopCalibrationMode() {
  calibrationMode = false;
  Serial.println("=== MODO CALIBRACIÓN FINALIZADO ===");
  drawIP();
}

void saveCalibrationIfReady(uint8_t acc) {
  if (acc == 3) {
    Serial.println("✔ Calibración lista. Guardando...");
    myIMU.saveCalibration();
  } else {
    Serial.printf("❌ Calibración insuficiente (%u).\n", acc);
  }
}

// ===========================================================
// WIFI
// ===========================================================
void connectWiFiInitial() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint8_t dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    drawConnecting(dots);
    dots = (dots + 1) % 4;
    delay(400);
  }

  Serial.println("✔ WiFi conectado");
  Serial.println(WiFi.localIP());
  drawIP();

  wifiPreviouslyConnected = true;
  wifiConnectedTime = millis();
}

void checkWiFi() {
  unsigned long now = millis();
  if (now - lastWifiCheck < 4000) return;
  lastWifiCheck = now;

  if (WiFi.status() != WL_CONNECTED) {
    if (wifiPreviouslyConnected) {
      Serial.println("✖ WiFi desconectado");
    }
    WiFi.reconnect();
    wifiPreviouslyConnected = false;
  }
  else if (!wifiPreviouslyConnected) {
    Serial.println("✔ WiFi reconectado");
    Serial.println(WiFi.localIP());
    drawIP();
    wifiPreviouslyConnected = true;
  }
}

// ===========================================================
// SETUP
// ===========================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  // OLED
  u8g2.begin();
  u8g2.setContrast(255);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN, 400000);

  // IMU
  Serial.println("Iniciando BNO08X...");
  if (!myIMU.begin()) {
    drawError("NO IMU");
    while (1);
  }

  myIMU.setCalibrationConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
  myIMU.enableRotationVector(10);

  // WIFI
  connectWiFiInitial();

  // LittleFS
  if (!LittleFS.begin(true)) {
    drawError("FS FAIL");
    while (1);
  }

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.addHandler(&events);

  // Endpoint calibración
  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request) {
    startCalibrationMode();
    request->send(200, "text/plain", "OK");
  });

  // Endpoint guardado
  server.on("/saveCal", HTTP_GET, [](AsyncWebServerRequest *request) {
    uint8_t acc = myIMU.getMagAccuracy();
    saveCalibrationIfReady(acc);
    request->send(200, "text/plain", "OK");
  });

  server.begin();
}

// ===========================================================
// LOOP PRINCIPAL
// ===========================================================
void loop() {
  checkWiFi();

  unsigned long now = millis();

  // Envío SSE
  if (WiFi.status() == WL_CONNECTED && now - previousSend >= sendInterval) {
    previousSend = now;

    if (myIMU.getSensorEvent()) {

      if (myIMU.getSensorEventID() == SENSOR_REPORTID_ROTATION_VECTOR) {
        float q_i = myIMU.getQuatI();
        float q_j = myIMU.getQuatJ();
        float q_k = myIMU.getQuatK();
        float q_r = myIMU.getQuatReal();
        uint8_t acc = myIMU.getMagAccuracy();

        // SSE JSON
        StaticJsonDocument<200> doc;
        doc["x"] = q_i;
        doc["y"] = q_j;
        doc["z"] = q_k;
        doc["w"] = q_r;
        doc["calib"] = acc;

        char buffer[200];
        serializeJson(doc, buffer);
        events.send(buffer, "quat", millis());

        // Auto-calibración
        if (calibrationMode && acc == 3) {
          saveCalibrationIfReady(acc);
          stopCalibrationMode();
        }
      }
    }
  }
}
