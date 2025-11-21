#include <WiFi.h>
#include <LittleFS.h>
#include <esp_task_wdt.h>
#include "ConfigSettings.h"
#include "Network.h"
#include "Web.h"
#include "Sockets.h"
#include "Utils.h"
#include "Somfy.h"
#include "MQTT.h"
#include "GitOTA.h"

// --- Ajout bouton physique ---
#define BUTTON_PIN 14  // Choisis une GPIO libre (évite celles utilisées par le CC1101)
#define DEBOUNCE_MS 200
unsigned long lastPress = 0;

ConfigSettings settings;
Web webServer;
SocketEmitter sockEmit;
Network net;
rebootDelay_t rebootDelay;
SomfyShadeController somfy;
MQTTClass mqtt;
GitUpdater git;

uint32_t oldheap = 0;
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("Startup/Boot....");
  Serial.println("Mounting File System...");
  if(LittleFS.begin()) Serial.println("File system mounted successfully");
  else Serial.println("Error mounting file system");
  settings.begin();
  if(WiFi.status() == WL_CONNECTED) WiFi.disconnect(true);
  delay(10);
  Serial.println();
  webServer.startup();
  webServer.begin();
  delay(1000);
  net.setup();  
  somfy.begin();
  //git.checkForUpdate();
  esp_task_wdt_init(7, true); //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL); //add current thread to WDT watch

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("Bouton physique initialisé sur GPIO 14");

}

void loop() {
  // put your main code here, to run repeatedly:
  //uint32_t heap = ESP.getFreeHeap();
  if(rebootDelay.reboot && millis() > rebootDelay.rebootTime) {
    Serial.print("Rebooting after ");
    Serial.print(rebootDelay.rebootTime);
    Serial.println("ms");
    net.end();
    ESP.restart();
    return;
  }
  uint32_t timing = millis();
  
  net.loop();
  if(millis() - timing > 100) Serial.printf("Timing Net: %ldms\n", millis() - timing);
  timing = millis();
  esp_task_wdt_reset();
  somfy.loop();
  if(millis() - timing > 100) Serial.printf("Timing Somfy: %ldms\n", millis() - timing);
  timing = millis();
  esp_task_wdt_reset();
  if(net.connected() || net.softAPOpened) {
    if(!rebootDelay.reboot && net.connected() && !net.softAPOpened) {
      git.loop();
      esp_task_wdt_reset();
    }
    webServer.loop();
    esp_task_wdt_reset();
    if(millis() - timing > 100) Serial.printf("Timing WebServer: %ldms\n", millis() - timing);
    esp_task_wdt_reset();
    timing = millis();
    sockEmit.loop();
    if(millis() - timing > 100) Serial.printf("Timing Socket: %ldms\n", millis() - timing);
    esp_task_wdt_reset();
    timing = millis();
  }
  if(rebootDelay.reboot && millis() > rebootDelay.rebootTime) {
    net.end();
    ESP.restart();
  }
  
// --- Lecture du bouton physique ---
  if (digitalRead(BUTTON_PIN) == LOW && millis() - lastPress > DEBOUNCE_MS) {
    lastPress = millis();

    // Récupère l'appareil Somfy par son nom
    SomfyShade* shade = SomfyRemote::getShadeByName("Portail Voiture");  // ← remplace "Portail" par le nom exact
    if (shade != nullptr) {
      shade->sendCommand(My);  // ou Up / Down selon la commande voulue
      Serial.println("Commande envoyée à 'Portail Voiture' via bouton physique !");
    } else {
      Serial.println("⚠️  Appareil 'Portail Voiture' introuvable !");
    }
  esp_task_wdt_reset();
}
