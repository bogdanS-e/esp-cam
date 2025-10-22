#ifndef CAR_H
#define CAR_H

#include "esp_camera.h"
#include <ESP32Servo.h>

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

#define SERVO_PIN 2
#define FLASH_PIN 4

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
  Car() : flashState(false), currentAngle(90), targetAngle(90), lastUpdate(0) {}

  esp_err_t init() {
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);

    servoX.attach(SERVO_PIN);
    servoX.write(90);

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

  bool getFlashState() const { return flashState; }

  void moveForward() { Serial.println("Forward"); }
  void moveBackward() { Serial.println("Backward"); }
  void turnLeft() { Serial.println("Left"); }
  void turnRight() { Serial.println("Right"); }
  void stop() { Serial.println("Stop"); }

  void setCameraX(int x) {
    x = constrain(x, -100, 100);
    targetAngle = map(x, -100, 100, 0, 180);
    Serial.printf("Target camera angle: %d\n", targetAngle);
  }

  void updateServo() {
    unsigned long now = millis();
    const int stepDelay = 5;
    if (now - lastUpdate < stepDelay)
      return;
    lastUpdate = now;

    if (currentAngle == targetAngle)
      return;

    int diff = targetAngle - currentAngle;

    int step = constrain(abs(diff) / 4 + 1, 1, 8);
    if (diff > 0)
      currentAngle += step;
    else
      currentAngle -= step;

    if ((diff > 0 && currentAngle > targetAngle) ||
        (diff < 0 && currentAngle < targetAngle))
      currentAngle = targetAngle;

    servoX.write(currentAngle);
  }

private:
  bool flashState;
  Servo servoX;

  int currentAngle;
  int targetAngle;
  unsigned long lastUpdate;

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
