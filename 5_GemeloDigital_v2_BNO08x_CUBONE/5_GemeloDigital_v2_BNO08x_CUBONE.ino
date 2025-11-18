// ===========================================================
// LIBRERÍAS
// ===========================================================
#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "SparkFun_BNO08x_Arduino_Library.h"
BNO08x myIMU;


// ===========================================================
// PINES ESP32-S3
// ===========================================================
#define SDA_PIN 4
#define SCL_PIN 5

#define RGB_PIN 21

extern "C" void neopixelWrite(uint8_t pin, uint8_t red, uint8_t green, uint8_t blue);
static inline void setLED(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(RGB_PIN, r, g, b);
}


// ===========================================================
// WIFI + SSE
// ===========================================================
const char* ssid = "Mi_casa";
const char* password = "ramses123";

AsyncWebServer server(80);
AsyncEventSource events("/events");

unsigned long previousSend = 0;
const unsigned long sendInterval = 50; // 20 Hz


// ===========================================================
// ESTADOS WIFI
// ===========================================================
bool wifiPreviouslyConnected = false;
unsigned long wifiConnectedTime = 0;
unsigned long lastWifiCheck = 0;
bool ledOffAfterDelay = false;


// ===========================================================
// MODO CALIBRACIÓN
// ===========================================================
bool calibrationMode = false;


// ===========================================================
// FUNCIONES DE CALIBRACIÓN
// ===========================================================
void startCalibrationMode() {
  calibrationMode = true;
  Serial.println("=== MODO CALIBRACIÓN ACTIVADO ===");
  setLED(64, 32, 0); // naranja
}

void stopCalibrationMode() {
  calibrationMode = false;
  Serial.println("=== MODO CALIBRACIÓN FINALIZADO ===");
  setLED(0, 0, 0);
}

void saveCalibrationIfReady(uint8_t acc) {
  if (acc == 3) {
    Serial.println("✔ Calibración lista (acc = 3). Guardando...");
    myIMU.saveCalibration();
    setLED(0, 64, 0);
    delay(250);
    setLED(0, 0, 0);
  } else {
    Serial.printf("❌ Calibración insuficiente (%u). Sigue moviendo el sensor.\n", acc);
  }
}


// ===========================================================
// WIFI
// ===========================================================
void connectWiFiInitial() {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, password);

  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    setLED(0, 0, 64);
    delay(200);
    setLED(0, 0, 0);
    delay(200);
    Serial.print(".");
  }

  Serial.println("\n✔ WiFi conectado");
  Serial.println(WiFi.localIP());
  setLED(0, 64, 0);

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
      setLED(0, 0, 64);
      wifiPreviouslyConnected = false;
    }
    WiFi.reconnect();
  }
  else if (!wifiPreviouslyConnected) {
    Serial.println("✔ WiFi reconectado");
    Serial.println(WiFi.localIP());
    setLED(0, 64, 0);

    wifiConnectedTime = now;
    wifiPreviouslyConnected = true;
    ledOffAfterDelay = false;
  }
}


// ===========================================================
// SETUP
// ===========================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  setLED(0, 0, 0);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN, 400000);

  // IMU
  Serial.println("Iniciando BNO08X...");

  if (!myIMU.begin()) {
    Serial.println("ERROR: No se detectó BNO08X.");
    while (1) {
      setLED(64, 0, 0);
      delay(150);
      setLED(0, 0, 0);
      delay(150);
    }
  }

  Serial.println("✔ BNO08X detectado");

  myIMU.setCalibrationConfig(SH2_CAL_ACCEL | SH2_CAL_GYRO | SH2_CAL_MAG);
  myIMU.enableRotationVector(10); // 100 Hz interno


  // WIFI
  connectWiFiInitial();

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("ERROR montando LittleFS");
    while (1);
  }

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
  server.addHandler(&events);

  // Endpoint activar calibración
  server.on("/calibrate", HTTP_GET, [](AsyncWebServerRequest *request) {
    startCalibrationMode();
    request->send(200, "text/plain", "OK");
  });

  // Endpoint guardar calibración manual
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

  // Apagar LED tras 10s conectado si no está calibrando
  if (!ledOffAfterDelay &&
      WiFi.status() == WL_CONNECTED &&
      (now - wifiConnectedTime > 10000)) {

    if (!calibrationMode) setLED(0, 0, 0);
    ledOffAfterDelay = true;
  }

  // Envío periódico de orientación SSE
  if (WiFi.status() == WL_CONNECTED && now - previousSend >= sendInterval) {
    previousSend = now;

    if (myIMU.getSensorEvent()) {

      // ROTATION VECTOR
      if (myIMU.getSensorEventID() == SENSOR_REPORTID_ROTATION_VECTOR) {

        float q_i = myIMU.getQuatI();
        float q_j = myIMU.getQuatJ();
        float q_k = myIMU.getQuatK();
        float q_r = myIMU.getQuatReal();

        uint8_t acc = myIMU.getMagAccuracy();  // ⭐ La única accuracy válida

        // Enviar por SSE
        StaticJsonDocument<200> doc;
        doc["x"] = q_i;
        doc["y"] = q_j;
        doc["z"] = q_k;
        doc["w"] = q_r;
        doc["calib"] = acc;

        char buffer[200];
        serializeJson(doc, buffer);
        events.send(buffer, "quat", millis());

        // Guardado automático
        if (calibrationMode && acc == 3) {
          saveCalibrationIfReady(acc);
          stopCalibrationMode();
        }
      }

      // MAGNETÓMETRO → accuracy real del sensor
      else if (myIMU.getSensorEventID() == SENSOR_REPORTID_MAGNETIC_FIELD) {
        // Se puede leer si lo necesitas
        // uint8_t accMag = myIMU.getMagAccuracy();
      }
    }
  }
}
