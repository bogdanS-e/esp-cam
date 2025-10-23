#ifndef CAR_H
#define CAR_H

#include "Motor.h"
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

// pins
#define RIGHT_MOTOR_PWM_1 12
#define RIGHT_MOTOR_PWM_2 13
#define RIGHT_MOTOR_CHANNEL_1 2
#define RIGHT_MOTOR_CHANNEL_2 4

#define LEFT_MOTOR_PWM_1 14
#define LEFT_MOTOR_PWM_2 15
#define LEFT_MOTOR_CHANNEL_1 3
#define LEFT_MOTOR_CHANNEL_2 5

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
  Car() : flashState(false),
          currentAngle(90),
          targetAngle(90),
          lastUpdate(0),
          motorL(LEFT_MOTOR_PWM_1, LEFT_MOTOR_PWM_2, LEFT_MOTOR_CHANNEL_1, LEFT_MOTOR_CHANNEL_2),
          motorR(RIGHT_MOTOR_PWM_1, RIGHT_MOTOR_PWM_2, RIGHT_MOTOR_CHANNEL_1, RIGHT_MOTOR_CHANNEL_2) {}

  esp_err_t init() {
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);

    initMotors();

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
    motorMax = 250;
    flashState = true;
  }

  void turnOffFlash() {
    digitalWrite(FLASH_PIN, LOW);
    motorMax = 200;
    flashState = false;
  }

  void toggleFlash() {
    flashState = !flashState;
    digitalWrite(FLASH_PIN, flashState ? HIGH : LOW);
    motorMax = flashState ? 200 : 255; // TODO remove it

    Serial.printf("Flash toggled to %s\n", flashState ? "ON" : "OFF");
  }

  bool getFlashState() const { return flashState; }

  void moveForward() {
    motorL.moveForward(motorMax);
    motorR.moveForward(motorMax);
  }

  void moveBackward() {
    motorL.moveBackward(motorMax);
    motorR.moveBackward(motorMax);
  }

  void turnLeft() { Serial.println("Left"); }
  void turnRight() { Serial.println("Right"); }
  
  void stop() {
    Serial.println("Stop");
    motorL.stop();
    motorR.stop();
  }

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

  uint8_t motorMax = 255;
  Motor motorL;
  Motor motorR;

  esp_err_t initCamera() {
    esp_err_t err = esp_camera_init(&camera_config);

    if (err != ESP_OK) {
      Serial.printf("Camera error: 0x%x\n", err);
      return err;
    }

    Serial.println("Camera initialized");
    return ESP_OK;
  }

  void initMotors() {
    motorL.begin();
    motorR.begin();
  }

  /* void processCommand(const char *command) {
    if (strcmp(command, "forward") == 0) {
      motorL.setSpeedPerc(motorMax);
      motorR.setSpeedPerc(motorMax);
    } else if (strcmp(command, "backward") == 0) {
      motorL.setSpeedPerc(-motorMax);
      motorR.setSpeedPerc(-motorMax);
    } else if (strcmp(command, "right") == 0) {
      motorL.setSpeedPerc(motorMax / 2);
      motorR.setSpeedPerc(-motorMax / 2);
    } else if (strcmp(command, "left") == 0) {
      motorL.setSpeedPerc(-motorMax / 2);
      motorR.setSpeedPerc(motorMax / 2);
    } else if (strcmp(command, "G") == 0) { // Forward left
      motorL.setSpeedPerc(motorMax / 4);
      motorR.setSpeedPerc(motorMax);
    } else if (strcmp(command, "H") == 0) { // Forward right
      motorL.setSpeedPerc(motorMax);
      motorR.setSpeedPerc(motorMax / 4);
    } else if (strcmp(command, "I") == 0) { // Backward left
      motorL.setSpeedPerc(-motorMax / 4);
      motorR.setSpeedPerc(-motorMax);
    } else if (strcmp(command, "J") == 0) { // Backward right
      motorR.setSpeedPerc(-motorMax / 4);
      motorL.setSpeedPerc(-motorMax);
    } else if (strcmp(command, "stop") == 0) {
      motorL.stop();
      motorR.stop();
    } else if (strcmp(command, "1") == 0) {
      motorMax = 50;
    } else if (strcmp(command, "2") == 0) {
      motorMax = 70;
    } else if (strcmp(command, "3") == 0) {
      motorMax = 80;
    } else if (strcmp(command, "4") == 0) {
      motorMax = 100;
    }
  } */
};

#endif // CAR_H
