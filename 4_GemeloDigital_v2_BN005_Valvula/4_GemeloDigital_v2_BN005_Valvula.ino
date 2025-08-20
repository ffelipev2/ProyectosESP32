/*
  Gemelo Digital – Bomba · Válvula · Tanque (solo ESP32 + BNO055)
  - Sirve /index.html desde LittleFS
  - Emite telemetría por SSE en /events:
    {
      quat:{x,y,z,w},
      euler:{h,r,p},           // heading, roll, pitch (deg)
      lin:{x,y,z},             // aceleración lineal m/s^2
      rms:{x,y,z,tot},         // RMS en g
      temp: <int °C>,
      impacts: <uint32>
    }
  - Emite evento "alarm" cuando detecta impacto (golpe / water hammer)
*/

#include <Wire.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_BNO055.h>
#include <Adafruit_Sensor.h>
#include <LittleFS.h>

// ===== Pines I2C ESP32-S3 Zero (ajusta si usas otros) =====
#define SDA_PIN 5
#define SCL_PIN 4

// ===== WiFi (ajusta credenciales) =====
const char* ssid     = "Mi_casa";
const char* password = "ramses123";

// ===== Web Server y SSE =====
AsyncWebServer server(80);
AsyncEventSource events("/events");

// ===== BNO055 =====
// Usa 0x29 como en tu proyecto. Si no detecta, prueba 0x28.
Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x29);
// Adafruit_BNO055 bno = Adafruit_BNO055(55, 0x28);

unsigned long previousSend = 0;
const unsigned long sendInterval = 40;  // ~25 Hz

// ===== Estado simple de vibración =====
struct VibStats {
  float rmsX = 0, rmsY = 0, rmsZ = 0, rms = 0;
  uint32_t lastImpactMs = 0;
  uint32_t impactCount = 0;
} vib;

const float IMPACT_G = 1.2f;            // umbral de pico (g) para impacto “water hammer”
const uint32_t IMPACT_DEBOUNCE = 150;   // ms

// ===== Utilidades =====
static inline float clampf(float v, float a, float b) {
  return (v < a) ? a : (v > b) ? b : v;
}

void calcRMS(const imu::Vector<3>& a_ms2) {
  // Pasar de m/s^2 a g
  const float gx = a_ms2.x() / 9.80665f;
  const float gy = a_ms2.y() / 9.80665f;
  const float gz = a_ms2.z() / 9.80665f;

  // RMS exponencial simple
  const float alpha = 0.12f; // suavizado
  vib.rmsX = sqrtf(alpha*gx*gx + (1-alpha)*vib.rmsX*vib.rmsX);
  vib.rmsY = sqrtf(alpha*gy*gy + (1-alpha)*vib.rmsY*vib.rmsY);
  vib.rmsZ = sqrtf(alpha*gz*gz + (1-alpha)*vib.rmsZ*vib.rmsZ);
  vib.rms  = sqrtf((vib.rmsX*vib.rmsX + vib.rmsY*vib.rmsY + vib.rmsZ*vib.rmsZ)/3.0f);

  // Impacto por pico instantáneo
  const float absmax = fmaxf(fabsf(gx), fmaxf(fabsf(gy), fabsf(gz)));
  const uint32_t now = millis();
  if (absmax >= IMPACT_G && (now - vib.lastImpactMs) > IMPACT_DEBOUNCE) {
    vib.lastImpactMs = now;
    vib.impactCount++;
    events.send("{\"event\":\"impact\"}", "alarm", now);
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);

  // BNO055
  Serial.println("Iniciando BNO055...");
  if (!bno.begin()) {
    Serial.println("No se pudo encontrar el BNO055. Verifica cableado y direccion 0x29/0x28.");
    while (1) delay(100);
  }
  bno.setExtCrystalUse(true);

  // IMPORTANTE: el enum está en el espacio global, no dentro de la clase
  // Puedes también comentar esta línea (begin suele dejar NDOF por defecto).
  bno.setMode(OPERATION_MODE_NDOF);

  Serial.println("BNO055 listo.");

  // WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Conectando WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.printf("\nWiFi OK. IP: %s\n", WiFi.localIP().toString().c_str());

  // LittleFS
  if (!LittleFS.begin(true)) {
    Serial.println("Error al montar LittleFS.");
    while (1) delay(100);
  }
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // SSE
  events.onConnect([](AsyncEventSourceClient* c){
    Serial.println("Cliente SSE conectado");
  });
  server.addHandler(&events);

  server.begin();
}

void loop() {
  const unsigned long now = millis();
  if (now - previousSend >= sendInterval) {
    previousSend = now;

    // Orientación
    imu::Quaternion quat = bno.getQuat();
    imu::Vector<3> euler = bno.getVector(Adafruit_BNO055::VECTOR_EULER); // heading, roll, pitch (deg)

    // Aceleración lineal (compensada de gravedad)
    imu::Vector<3> lin = bno.getVector(Adafruit_BNO055::VECTOR_LINEARACCEL);
    calcRMS(lin);

    // Temperatura interna del IMU
    int8_t tempC = bno.getTemp();

    // JSON unificado
    String data = "{";
    data += "\"quat\":{\"x\":" + String(quat.x(),6) + ",\"y\":" + String(quat.y(),6) + ",\"z\":" + String(quat.z(),6) + ",\"w\":" + String(quat.w(),6) + "},";
    data += "\"euler\":{\"h\":" + String(euler.x(),1) + ",\"r\":" + String(euler.y(),1) + ",\"p\":" + String(euler.z(),1) + "},";
    data += "\"lin\":{\"x\":" + String(lin.x(),3) + ",\"y\":" + String(lin.y(),3) + ",\"z\":" + String(lin.z(),3) + "},";
    data += "\"rms\":{\"x\":" + String(vib.rmsX,3) + ",\"y\":" + String(vib.rmsY,3) + ",\"z\":" + String(vib.rmsZ,3) + ",\"tot\":" + String(vib.rms,3) + "},";
    data += "\"temp\":" + String(tempC) + ",";
    data += "\"impacts\":" + String(vib.impactCount);
    data += "}";

    events.send(data.c_str(), "telemetry", now);
  }
}
