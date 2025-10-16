#ifndef CAR_H
#define CAR_H

#include "esp_camera.h"

// AI-Thinker cam config
#define CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22
#define FLASH_PIN 4

// Настройки камеры
static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,
    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count = 2};

class Car {
public:
  Car() {
    flashState = false;
  }

  esp_err_t init() {
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);
    return initCamera();
  }

  void capturePhoto() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }
    Serial.printf("Captured %d bytes\n", fb->len);
    esp_camera_fb_return(fb);
  }

  void turnOnFlash() {
    digitalWrite(FLASH_PIN, HIGH);
    flashState = true;
  }

  void turnOffFlash() {
    digitalWrite(FLASH_PIN, LOW);
    flashState = false;
  }

  void toggleFlash() {
    flashState = !flashState;
    digitalWrite(FLASH_PIN, flashState ? HIGH : LOW);
    Serial.printf("Flash toggled to %s\n", flashState ? "ON" : "OFF");
  }

  bool getFlashState() const {
    return flashState;
  }

  void moveForward() { Serial.println("Forward"); }
  void moveBackward() { Serial.println("Backward"); }
  void turnLeft() { Serial.println("Left"); }
  void turnRight() { Serial.println("Right"); }
  void stop() { Serial.println("Stop"); }

private:
  bool flashState;

  esp_err_t initCamera() {
    esp_err_t err = esp_camera_init(&camera_config);

    if (err != ESP_OK) {
      Serial.printf("Camera error: 0x%x\n", err);
      return err;
    }

    Serial.println("Camera initialized");

    return ESP_OK;
  }
};

#endif // CAR_H