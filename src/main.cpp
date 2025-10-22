#include "LittleFS.h"
#include "esp_timer.h"
#include "fb_gfx.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/soc.h"
#include <WiFi.h>

#include "Car.h"
#include "carServer.h"

Car car;

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

  // Connect to WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin("bogdan", "seredenko ");

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFailed to connect to WiFi!");
    return;
  }

  startCarServer();
}

void loop() {
  car.updateServo();
}