#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <LittleFS.h>

// Pines I2C para ESP32-S3 Zero
#define SDA_PIN 5
#define SCL_PIN 4

// WiFi
const char* ssid = "Mi_casa";
const char* password = "ramses123";

// Servidor Web y SSE
AsyncWebServer server(80);
AsyncEventSource events("/events");

// BNO055 en dirección 0x29
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29);

unsigned long previousSend = 0;
const unsigned long sendInterval = 40;  // 20Hz

void setup() {
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("Iniciando BNO055...");
  if (!bno.begin()) {
    Serial.println("No se pudo encontrar el BNO055.");
    while (1);
  }
  bno.setExtCrystalUse(true);
  Serial.println("BNO055 listo.");

  // WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Error al montar LittleFS.");
    return;
  }
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  events.onConnect([](AsyncEventSourceClient* client) {
    Serial.println("Cliente conectado SSE");
  });
  server.addHandler(&events);

  server.begin();
}

void loop() {
  unsigned long now = millis();
  if (now - previousSend >= sendInterval) {
    previousSend = now;

    imu::Quaternion quat = bno.getQuat();

    // Enviamos el quaternion directamente
    String data = String("{\"x\":") + quat.x() + ",\"y\":" + quat.y() + ",\"z\":" + quat.z() + ",\"w\":" + quat.w() + "}";
    events.send(data.c_str(), "accel", millis());
  }
}
