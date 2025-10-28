#include "LittleFS.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <WiFi.h>
#include <WiFiManager.h>

#include "Car.h"
#include "carServer.h"

Car car;
WiFiManager wm;

void setup() {
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  Serial.println();
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
  wm.setConnectTimeout(5);

  // Connect to WiFi
  Serial.print("Connecting to WiFi");

  // wm.resetSettings();

  wm.autoConnect("WiFi Car");
  startCarServer();
}

void loop() {
  car.updateServo();
  wm.process();
}