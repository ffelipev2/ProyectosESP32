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
//   WIFI (MODO AP)
// ===========================
AsyncWebServer server(80);
AsyncEventSource events("/events");


// ===========================
//   CONTROL ENVÍO SENSOR
// ===========================
unsigned long previousSend = 0;
const unsigned long sendInterval = 50; // 20Hz


// ===========================
//   OLED — PANTALLAS
// ===========================
void drawAPScreen()
{
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_5x8_tr);

  u8g2.setCursor(xOffset - 1, yOffset + 10);
  u8g2.print("WiFi AP listo");

  u8g2.setCursor(xOffset - 0.3, yOffset + 22);
  u8g2.print(WiFi.softAPIP().toString());

  u8g2.sendBuffer();
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
  myIMU.enableGameRotationVector(10); // ✅ SIN DRIFT

  // ---- LittleFS ----
  if (!LittleFS.begin(true)) {
    Serial.println("❌ ERROR LittleFS");
    while (1) {}
  }

  // ---- WIFI MODO AP ----
  WiFi.mode(WIFI_AP);
  WiFi.softAP("TrackerVR", "12345678"); // ✅ Nombre y contraseña del AP

  Serial.println("✅ AP Iniciado");
  Serial.print("🌐 Conéctate a: TrackerVR\n🔗 Abre: http://");
  Serial.println(WiFi.softAPIP());

  drawAPScreen();

  // ---- SERVIDOR WEB ----
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.serveStatic("/styles.css", LittleFS, "/styles.css");
  server.serveStatic("/app.js", LittleFS, "/app.js");
  server.serveStatic("/three.min.js", LittleFS, "/three.min.js");
  server.serveStatic("/GLTFLoader.js", LittleFS, "/GLTFLoader.js");
  server.serveStatic("/pokemon.glb", LittleFS, "/pokemon.glb");

  // ---- SSE ----
  events.onConnect([](AsyncEventSourceClient* client) {
    Serial.println("🌐 Cliente SSE conectado");
  });

  server.addHandler(&events);
  server.begin();

  Serial.println("✅ Servidor listo");
}


// ===========================
//   LOOP PRINCIPAL
// ===========================
void loop() {
  unsigned long now = millis();

  if (now - previousSend >= sendInterval) {
    previousSend = now;

    if (myIMU.getSensorEvent()) {

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
