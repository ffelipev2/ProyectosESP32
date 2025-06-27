#include <WiFi.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <LittleFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Pines y configuración del DHT22
#define DHTPIN 16
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE, 22);

// Pines y configuración del HC-SR04
#define TRIG_PIN 25
#define ECHO_PIN 26

// Configuración del LED
#define LED_PIN 2
bool ledState = false;

// Variables de tiempo
unsigned long lastSensorUpdate = 0;

// WiFi y servidor
AsyncWebServer server(80);
WebSocketsServer webSocket(81);

// Variables de los sensores
float temperatura = 0.0;
float humedad = 0.0;
float distancia = 0.0;

void setup() {
  Serial.begin(115200);
  dht.begin();

  // Inicialización del LED
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  // Inicialización del HC-SR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Configuración de WiFi
  WiFi.softAP("ESP32-AP", "12345678");
  IPAddress IP = WiFi.softAPIP();
  Serial.print("Dirección IP del AP: ");
  Serial.println(IP);

  // Inicialización del sistema de archivos
  if (!LittleFS.begin(true)) {
    Serial.println("Error montando LittleFS");
    return;
  } else {
    Serial.println("LittleFS montado correctamente");
  }

  // Configuración de las rutas
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Configuración del WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Inicia el servidor
  server.begin();
}

void loop() {
  // Manejar WebSocket
  webSocket.loop();

  // Actualizar sensores cada 1 segundo
  if (millis() - lastSensorUpdate >= 1000) {
    lastSensorUpdate = millis();

    // Leer distancia
    distancia = medirDistancia();

    // Leer temperatura y humedad
    temperatura = dht.readTemperature();
    humedad = dht.readHumidity();

    // Validar valores y armar JSON seguro
    String distanciaStr = isnan(distancia) ? "null" : String(distancia, 2);
    String temperaturaStr = isnan(temperatura) ? "null" : String(temperatura, 1);
    String humedadStr = isnan(humedad) ? "null" : String(humedad, 1);

    String sensorData = String("{\"distancia\":") + distanciaStr +
                        ",\"temperatura\":" + temperaturaStr +
                        ",\"humedad\":" + humedadStr + "}";

    // Enviar por WebSocket
    webSocket.broadcastTXT(sensorData);
    Serial.println(sensorData);  // Debug
  }
}

float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH, 30000); // timeout de 30 ms
  if (duracion == 0) {
    return NAN; // No se midió nada
  }
  float distancia = (duracion * 0.034) / 2;
  return distancia;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = String((char *)payload);
    if (message == "ledOn") {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      webSocket.broadcastTXT("{\"led\":\"on\"}");
    } else if (message == "ledOff") {
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      webSocket.broadcastTXT("{\"led\":\"off\"}");
    }
  }
}
