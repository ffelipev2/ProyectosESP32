#include <WiFi.h>
#include <FS.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <WebSocketsServer.h>

// Configuración del Access Point
const char* ssid = "ESP32-AP";
const char* password = "12345678";

// Pines del LED y HC-SR04
#define LED_PIN 2
#define TRIG_PIN 5
#define ECHO_PIN 18

// Crear servidor web y WebSocket
WebServer server(80);
WebSocketsServer webSocket(81);

// Variable para almacenar la distancia
float distancia = 0.0;
unsigned long lastDistanceUpdate = 0; // Para el temporizador

// Función para medir la distancia con el HC-SR04
float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duracion = pulseIn(ECHO_PIN, HIGH);
  return (duracion * 0.034) / 2; // Distancia en cm
}

// Función para manejar mensajes WebSocket
void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (type == WStype_TEXT) {
    String msg = String((char *)payload);
    if (msg == "ledOn") {
      digitalWrite(LED_PIN, HIGH);
      webSocket.sendTXT(num, "LED encendido");
    } else if (msg == "ledOff") {
      digitalWrite(LED_PIN, LOW);
      webSocket.sendTXT(num, "LED apagado");
    }
  }
}

void setup() {
  // Configuración de pines
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Serial.begin(115200);
  Serial.println("Iniciando ESP32...");

  // Iniciar SPIFFS
  if (!SPIFFS.begin(true)) {
    Serial.println("Error al montar SPIFFS");
    return;
  }

  // Configurar Access Point
  WiFi.softAP(ssid, password);
  Serial.println("Access Point configurado");
  Serial.println(WiFi.softAPIP());

  // Rutas para servir archivos estáticos
  server.on("/", HTTP_GET, []() {
    File file = SPIFFS.open("/index.html", "r");
    server.streamFile(file, "text/html");
    file.close();
  });
  server.on("/style.css", HTTP_GET, []() {
    File file = SPIFFS.open("/style.css", "r");
    server.streamFile(file, "text/css");
    file.close();
  });
  server.on("/script.js", HTTP_GET, []() {
    File file = SPIFFS.open("/script.js", "r");
    server.streamFile(file, "application/javascript");
    file.close();
  });

  // Iniciar el servidor web
  server.begin();
  Serial.println("Servidor iniciado");

  // Iniciar WebSocket
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  Serial.println("WebSocket iniciado");
}

void loop() {
  // Manejar solicitudes de clientes y WebSocket
  server.handleClient();
  webSocket.loop();

  // Actualizar la distancia cada 1 segundo y enviarla por WebSocket
  if (millis() - lastDistanceUpdate >= 1000) {
    lastDistanceUpdate = millis();
    distancia = medirDistancia();
    Serial.println("Distancia: " + String(distancia) + " cm");
    String distanciaStr = String(distancia);
    webSocket.broadcastTXT(distanciaStr);
  }
}
