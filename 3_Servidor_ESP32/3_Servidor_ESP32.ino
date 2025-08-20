#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <DHT.h>
#include <LittleFS.h>

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
AsyncWebSocket ws("/ws");   // WebSocket en la ruta /ws

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

  // Configuración de WiFi en modo estación
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // mejora tiempos de respuesta
  WiFi.begin("Mi_casa", "ramses123");
  Serial.print("Conectando a WiFi");

  unsigned long startAttemptTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 10000) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Conectado a WiFi");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ No se pudo conectar a WiFi");
  }

  // Inicialización del sistema de archivos
  if (!LittleFS.begin(true)) {
    Serial.println("Error montando LittleFS");
    return;
  } else {
    Serial.println("LittleFS montado correctamente");
  }

  // Configuración de las rutas HTTP
  server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

  // Configuración del WebSocket
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  // Inicia el servidor
  server.begin();
}

void loop() {
  // No es necesario llamar loop() como en WebSocketsServer
  // AsyncWebServer maneja todo en segundo plano automáticamente

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
    ws.textAll(sensorData);
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

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                      AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    // Construir el mensaje correctamente con el tamaño real (len)
    String message = "";
    for (size_t i = 0; i < len; i++) {
      message += (char)data[i];
    }

    Serial.print("Mensaje recibido: ");
    Serial.println(message);

    if (message == "ledOn") {
      ledState = true;
      digitalWrite(LED_PIN, HIGH);   // o LOW si es invertido
      ws.textAll("{\"led\":\"on\"}");
    } else if (message == "ledOff") {
      ledState = false;
      digitalWrite(LED_PIN, LOW);    // o HIGH si es invertido
      ws.textAll("{\"led\":\"off\"}");
    }
  }
}
