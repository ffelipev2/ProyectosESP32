#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <LittleFS.h>

Adafruit_MPU6050 mpu;

// Pines I2C para ESP32-S3 Zero
#define SDA_PIN 4
#define SCL_PIN 5

// Red WiFi
const char* ssid = "Mi_casa";
const char* password = "ramses123";

// Servidor web y SSE
AsyncWebServer server(80);
AsyncEventSource events("/events");

// Filtro complementario y temporización
float angleX = 0, angleY = 0;
float angleOffsetX = 0, angleOffsetY = 0;
unsigned long lastTime = 0;
unsigned long previousSend = 0;
const unsigned long sendInterval = 100;  // milisegundos

// Calibración web
void calibrar() {
  angleOffsetX = angleX;
  angleOffsetY = angleY;
  Serial.println("Calibración realizada desde la web:");
  Serial.println("Offset Ángulo X (pitch): " + String(angleOffsetX));
  Serial.println("Offset Ángulo Y (roll): " + String(angleOffsetY));
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!mpu.begin()) {
    Serial.println("Error al detectar MPU6050");
    while (true) delay(10);
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi conectado: " + WiFi.localIP().toString());

  if (!LittleFS.begin(true)) {
    Serial.println("Error al montar LittleFS");
    return;
  }

  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  events.onConnect([](AsyncEventSourceClient *client) {
    Serial.println("Cliente SSE conectado");
  });
  server.addHandler(&events);

  server.on("/calibrar", HTTP_GET, [](AsyncWebServerRequest *request){
    calibrar();
    request->send(200, "text/plain", "Calibrado");
  });

  server.begin();
}

void loop() {
  unsigned long now = millis();
  float dt = (now - lastTime) / 1000.0;
  lastTime = now;

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Calcular ángulos desde acelerómetro
  float accelAngleX = atan2(a.acceleration.y, a.acceleration.z) * 180 / PI;
  float accelAngleY = atan2(-a.acceleration.x, a.acceleration.z) * 180 / PI;

  // Filtro complementario
  angleX = 0.98 * (angleX + g.gyro.x * dt * 180 / PI) + 0.02 * accelAngleX;
  angleY = 0.98 * (angleY + g.gyro.y * dt * 180 / PI) + 0.02 * accelAngleY;

  // Enviar cada 100 ms
  if (now - previousSend >= sendInterval) {
    previousSend = now;

    float pitch = (angleX - angleOffsetX) * PI / 180.0;
    float roll  = (angleY - angleOffsetY) * PI / 180.0;

    String data = String("{\"x\":") + pitch +
                  ",\"y\":0," +
                  "\"z\":" + roll + "}";
    //Serial.println(data);
    events.send(data.c_str(), "accel", millis());
  }
}
