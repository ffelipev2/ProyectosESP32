#include <WiFi.h>
#include <WebSocketsServer.h>
#include <DHT.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

// Pines y configuración del DHT22
#define DHTPIN 4 // Cambiar al pin conectado al DHT22
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE, 22);

// Pines y configuración del HC-SR04
#define TRIG_PIN 27
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

  // Configuración del SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error montando SPIFFS");
    return;
  }

  // Configuración de las rutas
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  // Configuración del WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  // Inicia el servidor
  server.begin();
}

void loop() {
  // Manejar WebSocket
  webSocket.loop();

  // Actualizar los sensores cada 1 segundo
  if (millis() - lastSensorUpdate >= 1000) {
    lastSensorUpdate = millis();

    // Leer datos del HC-SR04
    distancia = medirDistancia();

    // Leer datos del DHT22
    temperatura = dht.readTemperature();
    humedad = dht.readHumidity();

    // Enviar datos por WebSocket
    String sensorData = String("{\"distancia\":") + distancia +
                        ",\"temperatura\":" + temperatura +
                        ",\"humedad\":" + humedad + "}";
    webSocket.broadcastTXT(sensorData);

    Serial.println(sensorData); // Debug
  }
}

float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duracion = pulseIn(ECHO_PIN, HIGH);
  float distancia = (duracion * 0.034) / 2; // Conversión a cm
  return distancia;
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String message = String((char *)payload);
    if (message == "ledOn") {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);
      webSocket.broadcastTXT("LED encendido");
    } else if (message == "ledOff") {
      ledState = false;
      digitalWrite(LED_PIN, LOW);
      webSocket.broadcastTXT("LED apagado");
    }
  }
}
