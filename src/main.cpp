#include "LittleFS.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "Car.h"
#include "carServer.h"

Car car;
WiFiManager wm;
bool mDNSStarted = false;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println("=== ESP32-CAM with WebSocket Flash Control ===");

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    return;
  }
  Serial.println("LittleFS mounted successfully");

  if (car.init() != ESP_OK) {
    Serial.println("Reboot in 5 seconds...");
    delay(5000);
    ESP.restart();
  }

  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  wm.setCaptivePortalEnable(false);
  wm.setConnectTimeout(8);

  // wm.resetSettings();

  wm.autoConnect("WiFi Car");
  startCarServer();
  /* pinMode(33, OUTPUT);
  digitalWrite(33, LOW); */
}

void setupMDNS() {
  if (MDNS.begin("car")) {
    Serial.println("mDNS responder started");
    MDNS.addService("car", "tcp", 80);

    mDNSStarted = true;

    MDNS.addServiceTxt("car", "tcp", "INSTRUCTION", "Zaydi po adresu http://car.local:82 yesli ne rabotayet, to http://<IP_ADRES_MASHINKI>:82 (s telefona srabativaet, a na Windows nado ustanovit' Bonjour paket, chtoby zarabotal mDNS https://support.apple.com/ru-ru/106380)");
    MDNS.addServiceTxt("car", "tcp", "Fallback link", "http://<IP_ADRES_MASHINKI>:82");
    MDNS.addServiceTxt("car", "tcp", "Main link", "http://car.local:82");
    MDNS.addServiceTxt("car", "tcp", "Device", "wificar");
    MDNS.addServiceTxt("car", "tcp", "Model", "esp32-cam ai thinker");
    MDNS.addServiceTxt("car", "tcp", "Version", "1.0");
    MDNS.addServiceTxt("car", "tcp", "Author", "Bogdan Seredenko");

    return;
  }

  Serial.println("Error setting up MDNS responder!");
}

void loop() {
  car.tick();
  wm.process();

  if (WiFi.status() == WL_CONNECTED && !mDNSStarted) {
    Serial.print("WiFi connected! IP address: ");
    Serial.println(WiFi.localIP());
    setupMDNS();
  }

  if (WiFi.status() != WL_CONNECTED && mDNSStarted) {
    mDNSStarted = false;
    Serial.println("WiFi disconnected, mDNS stopped");
  }
}