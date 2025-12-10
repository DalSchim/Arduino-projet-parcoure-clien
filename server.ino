// ======= SERVEUR ESP8266 : SEQUENCE 1 -> 2 -> 3 ... + BOUTON WEB =======

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>

extern "C" {
  #include "user_interface.h"
}

// --- CONFIG WIFI ---
const char* SSID_AP     = "CHAIN_SERVER";
const char* PASSWORD_AP = "12345678";

// Nombre total de clients dans la chaîne (ajustez selon votre montage)
const int NUM_CLIENTS = 4;

// LED témoin serveur (GPIO2 = D4)
const int LED_PIN = 2;

ESP8266WebServer server(80);

// Pour affichage des clients connectés
int lastConnectedCount = -1;

// État de la séquence
bool sequenceRunning = false;
int  currentClient   = 0;   // 0 = pas encore démarré

// -------- IP du client à partir de son ID --------
// ID 1 -> 192.168.4.101
// ID 2 -> 192.168.4.102
IPAddress getClientIP(int id) {
  return IPAddress(192, 168, 4, 100 + id);
}


// ============================================================================
//               DETECTION DES CLIENTS CONNECTÉS (log Série)
// ============================================================================
void checkClients() {
  int count = wifi_softap_get_station_num();
  if (count == lastConnectedCount) return;

  Serial.println("\n=== Mise à jour clients connectés ===");
  Serial.print("Nombre : ");
  Serial.println(count);

  struct station_info *stat = wifi_softap_get_station_info();

  if (stat == NULL) {
    Serial.println("Aucun client connecté.");
  } else {
    while (stat != NULL) {
      IPAddress ip(stat->ip.addr);

      char macStr[18];
      sprintf(macStr, "%02X:%02X:%02X:%02X:%02X:%02X",
        stat->bssid[0], stat->bssid[1], stat->bssid[2],
        stat->bssid[3], stat->bssid[4], stat->bssid[5]);

      Serial.print("→ IP : ");
      Serial.print(ip);
      Serial.print(" | MAC : ");
      Serial.println(macStr);

      stat = STAILQ_NEXT(stat, next);
    }
  }

  lastConnectedCount = count;
}


// ============================================================================
//                    FONCTION : déclencher un client
// ============================================================================
void triggerClient(int id) {
  if (id < 1 || id > NUM_CLIENTS) {
    Serial.println("ID client invalide, on ne déclenche pas.");
    return;
  }

  IPAddress ip = getClientIP(id);
  WiFiClient client;
  HTTPClient http;

  String url = "http://" + ip.toString() + "/trigger";

  Serial.print("→ Déclenche CLIENT ");
  Serial.print(id);
  Serial.print(" (");
  Serial.print(ip);
  Serial.println(")");

  if (http.begin(client, url)) {
    int code = http.GET();
    Serial.print("Réponse client ");
    Serial.print(id);
    Serial.print(" : ");
    Serial.println(code);
    http.end();
  } else {
    Serial.println("http.begin() a échoué");
  }
}


// ============================================================================
//                       ROUTE : /button?id=N
//       (appelée par le client quand la bille appuie sur son bouton)
// ============================================================================
void handleButton() {
  int fromId = -1;
  if (server.hasArg("id")) {
    fromId = server.arg("id").toInt();
  }

  Serial.print("\n/bouton reçu de CLIENT ");
  Serial.println(fromId);

  if (!sequenceRunning) {
    Serial.println("Séquence non démarrée -> on ignore le bouton.");
    server.send(200, "text/plain", "Sequence not running");
    return;
  }

  Serial.print("Étape actuelle (currentClient) = ");
  Serial.println(currentClient);

  // Vérif soft : on peut vérifier que fromId == currentClient
  if (fromId != currentClient) {
    Serial.println("⚠ Attention : id reçu différent de l'étape courante !");
  }

  // Si on est déjà au dernier client, on STOPPE la séquence
  if (currentClient >= NUM_CLIENTS) {
    Serial.println("Dernier client atteint, fin de séquence.");
    sequenceRunning = false;
    currentClient   = 0;
    server.send(200, "text/plain", "End of sequence");
    return;
  }

  // Sinon on passe au client suivant
  currentClient++;
  Serial.print("Prochain client dans la séquence : ");
  Serial.println(currentClient);

  triggerClient(currentClient);

  server.send(200, "text/plain", "OK");
}


// ============================================================================
//                       ROUTE : /start
//           (démarre la séquence en partant du client 1)
// ============================================================================
void handleStart() {
  Serial.println("\n=== DEMARRAGE SEQUENCE (depuis page web) ===");

  sequenceRunning = true;
  currentClient   = 1;

  triggerClient(currentClient);

  // Réponse simple (la page HTML principale se recharge ensuite)
  server.send(200, "text/plain", "Sequence started at client 1");
}


// ============================================================================
//                       ROUTE : /stop
//           (arrête la séquence en cours depuis la page web)
// ============================================================================
void handleStop() {
  Serial.println("\n=== ARRET SEQUENCE (depuis page web) ===");
  sequenceRunning = false;
  currentClient   = 0;
  server.send(200, "text/plain", "Sequence stopped");
}


// ============================================================================
//                       ROUTE : /ping (pour les clients)
// ============================================================================
void handlePing() {
  if (server.hasArg("id")) {
    Serial.print("Ping reçu du client ");
    Serial.println(server.arg("id"));
  }
   server.send(200, "text/plain", "pong");
}


// ============================================================================
//                     ROUTE : /trigger?id=N (manuel)
//         (pour tester l'ouverture d'un client sans la séquence)
// ============================================================================
void handleTriggerManual() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain", "Missing id");
    return;
  }

  int id = server.arg("id").toInt();
  Serial.print("\n/TRIGGER manuel pour client ");
  Serial.println(id);

  triggerClient(id);
  server.send(200, "text/plain", "Manual trigger sent");
}


// ============================================================================
//                       PAGE WEB PRINCIPALE
// ============================================================================
void handleRoot() {
  String page = "";
  page += "<html><head><meta charset='utf-8'><title>Serveur CHAIN</title></head><body>";
  page += "<h1>Serveur CHAIN</h1>";

  page += "<p>SSID AP : <b>";
  page += SSID_AP;
  page += "</b></p>";

  page += "<p>État de la séquence : ";
  page += (sequenceRunning ? "<b style='color:green;'>EN COURS</b>" : "<b style='color:red;'>ARRETÉE</b>");
  page += "</p>";

  page += "<p>Client courant : <b>";
  page += String(currentClient);
  page += "</b></p>";

  // -------- BOUTON POUR LANCER LA SÉQUENCE ----------
  page += "<form action=\"/start\" method=\"GET\">";
  page += "<button type=\"submit\" style=\"font-size:24px; padding:10px 20px; margin-top:10px;\">Démarrer la séquence</button>";
  page += "</form>";

  page += "<form action=\"/stop\" method=\"GET\" style='margin-top:10px;'>";
  page += "<button type=\"submit\" style=\"font-size:18px; padding:8px 16px; background:#c0392b; color:white; border:none;\">Arrêter la séquence</button>";
  page += "</form>";

  page += "<p style='margin-top:20px; font-size:12px; color:#666;'>"
          "Ce bouton lance la séquence à partir du client 1 (1 → 2 → 3 → ...).</p>";

  // --- Déclenchement manuel des clients ---
  page += "<h3>Déclenchement manuel des clients</h3>";
  page += "<p>Utilisez ces boutons pour tester l'ouverture de chaque servo indépendamment de la séquence.</p>";

  for (int i = 1; i <= NUM_CLIENTS; i++) {
    page += "<form action=\"/trigger\" method=\"GET\" style='display:inline-block; margin:4px;'>";
    page += "<input type='hidden' name='id' value='" + String(i) + "'>";
    page += "<button type=\"submit\" style=\"padding:6px 12px; font-size:16px;\">Client " + String(i) + "</button>";
    page += "</form>";
  }

  page += "</body></html>";

  server.send(200, "text/html", page);
}


// ============================================================================
//                               SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);  // LED ON = serveur actif (active LOW)

  Serial.println("\n=== SERVEUR CHAIN (ordre 1->2->3...) ===");

  WiFi.mode(WIFI_AP);
  WiFi.softAP(SSID_AP, PASSWORD_AP);

  Serial.print("AP SSID : ");
  Serial.println(SSID_AP);
  Serial.print("IP serveur : ");
  Serial.println(WiFi.softAPIP());

  server.on("/",      handleRoot);
  server.on("/start", handleStart);
  server.on("/stop",  handleStop);
  server.on("/button",handleButton);
  server.on("/ping",  handlePing);
  server.on("/trigger", handleTriggerManual);

  server.begin();
  Serial.println("Serveur HTTP démarré.");
}


// ============================================================================
//                               LOOP
// ============================================================================
void loop() {
  server.handleClient();
  checkClients();
}
