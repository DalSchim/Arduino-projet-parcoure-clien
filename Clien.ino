// ======= CLIENT ESP8266 : PING + LED ONLINE + BOUTON + MOTEUR =======

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

const int CLIENT_ID = 4;

// SERVEUR
const char* SSID_SERVER   = "CHAIN_SERVER";
const char* PASSWORD_SERV = "12345678";

IPAddress IP_SERVER(192,168,4,1);

// IP fixe du client
IPAddress IP_CLIENT(192,168,4,100 + CLIENT_ID);
IPAddress GATEWAY(192,168,4,1);
IPAddress SUBNET(255,255,255,0);

// LED ONLINE
const int LED_PIN = 2;  // GPIO2 = D4

// BOUTON + MOTEUR
const int BUTTON_PIN = 5;   // D1
const int MOTOR_PIN  = 14;  // D5

ESP8266WebServer server(80);

unsigned long lastPing = 0;
bool serverReachable = false;

// ---- Moteur ----
void handleTrigger() {
  Serial.println("TRIGGER → moteur ON");
  digitalWrite(MOTOR_PIN, HIGH);
  delay(800);
  digitalWrite(MOTOR_PIN, LOW);
  server.send(200,"text/plain","OK");
}

// ---- Signal bouton ----
void notifyServer() {
  if (!serverReachable) {
    Serial.println("Serveur injoignable → pas d’envoi");
    return;
  }

  WiFiClient client;
  HTTPClient http;

  String url = "http://" + IP_SERVER.toString() + "/button?id=" + String(CLIENT_ID);
  http.begin(client, url);
  http.GET();
  http.end();
}

// ---- PING ----
void sendPing() {
  WiFiClient client;
  HTTPClient http;

  String url = "http://" + IP_SERVER.toString() + "/ping?id=" + String(CLIENT_ID);

  if (http.begin(client, url)) {
    int code = http.GET();

    if (code == 200) {
      serverReachable = true;
      Serial.println("Ping OK → serveur disponible");
    } else {
      serverReachable = false;
      Serial.println("Ping FAIL → pas de réponse");
    }

    http.end();
  } else {
    serverReachable = false;
    Serial.println("Ping impossible");
  }

  // LED selon état serveur
  if (serverReachable) {
    digitalWrite(LED_PIN, LOW);  // LED ON (active LOW)
  } else {
    digitalWrite(LED_PIN, HIGH); // LED OFF
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);
  digitalWrite(LED_PIN, HIGH);  // LED OFF au départ

  // Connexion WiFi
  WiFi.mode(WIFI_STA);
  WiFi.config(IP_CLIENT, GATEWAY, SUBNET);
  WiFi.begin(SSID_SERVER, PASSWORD_SERV);

  Serial.println("Connexion au serveur...");

  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
  }

  Serial.println("Client connecté !");
  digitalWrite(LED_PIN, LOW); // LED ON

  server.on("/trigger", handleTrigger);
  server.begin();
}

void loop() {
  server.handleClient();

  // --- PING toutes les 2 secondes ---
  if (millis() - lastPing > 2000) {
    lastPing = millis();
    sendPing();
  }

  // Bouton → envoi au serveur
  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Bouton appuyé → notify");
    notifyServer();
    delay(400);
    while (digitalRead(BUTTON_PIN)==LOW) delay(10);
  }
}
