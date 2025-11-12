#include "LittleFS.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "fb_gfx.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "config.h"
#include "Car.h"
#include "carServer.h"
#include "customApSuccess.h"

#define LED_PIN 33
#define LEDC_CHANNEL 0
#define LEDC_FREQ 5000

Car car;
WiFiManager wm;
bool mDNSStarted = false;
extern bool isClientActive;

void ledTask(void *param) {
  unsigned long lastBlink = 0;
  unsigned long lastFade = 0;
  bool ledState = false;
  int fadeValue = 0;
  int fadeDirection = 1;

  const int fadeStep = 5;
  const int fadeIntervalMs = 30;

  for (;;) {
    unsigned long now = millis();

    if (isClientActive) {
      if (now - lastFade >= fadeIntervalMs) {
        lastFade = now;
        fadeValue += fadeDirection * fadeStep;

        if (fadeValue >= 220) {
          fadeValue = 220;
          fadeDirection = -1;
        } else if (fadeValue <= 0) {
          fadeValue = 0;
          fadeDirection = 1;
        }

        ledcWrite(LEDC_CHANNEL, fadeValue);
      }
    } else {
      wifi_mode_t mode = WiFi.getMode();
      int interval = 2000;

      if (mode == WIFI_MODE_AP)
        interval = 250;
      else if (mode == WIFI_MODE_STA)
        interval = 1000;

      if (now - lastBlink >= (unsigned long)interval) {
        lastBlink = now;
        ledState = !ledState;
        ledcWrite(LEDC_CHANNEL, ledState ? 255 : 0);
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println("=== ESP32-CAM with WebSocket Flash Control ===");
  pinMode(33, OUTPUT);

  // blink indicate boot started
  blink(LED_PIN, 1, 1000);

  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS mount failed!");
    blink(LED_PIN, 3); // error indication

    return;
  }

  Serial.println("LittleFS mounted successfully");

  if (car.init() != ESP_OK) {
    Serial.println("Reboot in 3 seconds...");
    blink(LED_PIN, 3); // error indication

    delay(3000);
    ESP.restart();

    return;
  }

  WiFi.mode(WIFI_STA);
  wm.setConfigPortalBlocking(false);
  wm.setCaptivePortalEnable(false);
  wm.setConnectTimeout(8);
  wm.setDarkMode(true);
  addCustomWiFiManagerUI(wm);

  wm.autoConnect("WiFi Car");

  startCarServer();

  blink(LED_PIN, 1, 1000); // successful boot indication

  ledcSetup(LEDC_CHANNEL, LEDC_FREQ, 8);
  ledcAttachPin(LED_PIN, LEDC_CHANNEL);
  xTaskCreate(
      ledTask,
      "LedTask",
      1024,
      nullptr,
      1,
      nullptr);
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