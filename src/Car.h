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
    .frame_size = FRAMESIZE_UXGA,
    .jpeg_quality = 10,
    .fb_count = 2,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_LATEST};

class Car {
public:
  Car()
      : flashState(false),
        currentAngleX(90),
        targetAngleX(90),
        lastUpdate(0),
        lastCommandTime(0),
        motorStopped(true),
        motorL(LEFT_MOTOR_PWM_1, LEFT_MOTOR_PWM_2, LEFT_MOTOR_CHANNEL_1, LEFT_MOTOR_CHANNEL_2),
        motorR(RIGHT_MOTOR_PWM_1, RIGHT_MOTOR_PWM_2, RIGHT_MOTOR_CHANNEL_1, RIGHT_MOTOR_CHANNEL_2) {}

  void preinit() {
    digitalWrite(LEFT_MOTOR_PWM_1, LOW);
    digitalWrite(LEFT_MOTOR_PWM_2, LOW);
    digitalWrite(RIGHT_MOTOR_PWM_1, LOW);
    digitalWrite(RIGHT_MOTOR_PWM_2, LOW);
  }

  esp_err_t init() {
    pinMode(FLASH_PIN, OUTPUT);
    digitalWrite(FLASH_PIN, LOW);

    initMotors();

    servoX.attach(SERVO_PIN);
    servoX.write(90);

    lastCommandTime = nowMs();

    return initCamera();
  }

  void onCommand() {
    lastCommandTime = nowMs();
    motorStopped = false;
  }

  void tick() {
    updateServo();
    tickAutoStop();
    motorL.tick();
    motorR.tick();
  }

  void toggleFlash() {
    flashState = !flashState;
    digitalWrite(FLASH_PIN, flashState ? HIGH : LOW);
  }

  bool getFlashState() {
    return flashState;
  }

  void moveForward() {
    onCommand();
    motorL.moveForward(motorMax);
    motorR.moveForward(motorMax);
  }

  void moveBackward() {
    onCommand();
    motorL.moveBackward(motorMax);
    motorR.moveBackward(motorMax);
  }

  void turnRight() {
    onCommand();
    motorL.moveForward(motorMax);
    motorR.moveBackward(motorMax);
  }

  void turnLeft() {
    onCommand();
    motorL.moveBackward(motorMax);
    motorR.moveForward(motorMax);
  }

  void moveForwardLeft() {
    onCommand();
    motorL.moveForward(motorMax / 1.5);
    motorR.moveForward(motorMax);
  }

  void moveForwardRight() {
    onCommand();
    motorL.moveForward(motorMax);
    motorR.moveForward(motorMax / 1.5);
  }

  void moveBackwardLeft() {
    onCommand();
    motorL.moveBackward(motorMax / 1.5);
    motorR.moveBackward(motorMax);
  }

  void moveBackwardRight() {
    onCommand();
    motorL.moveBackward(motorMax);
    motorR.moveBackward(motorMax / 1.5);
  }

  void stop() {
    motorL.stop();
    motorR.stop();
    motorStopped = true;
  }

  void setCameraX(int x) {
    x = constrain(x, -100, 100);
    targetAngleX = map(x, -100, 100, 0, 180);
  }

private:
  bool flashState;
  Servo servoX;

  int currentAngleX;
  int targetAngleX;
  uint64_t lastUpdate;

  uint8_t motorMax = 255;
  Motor motorL;
  Motor motorR;

  uint64_t lastCommandTime;
  bool motorStopped;

  void updateServo() {
    uint64_t now = nowMs();
    const int stepDelay = 5;

    int64_t diff = elapsedSince(lastUpdate);

    if (diff < stepDelay)
      return;

    lastUpdate = now;

    if (currentAngleX == targetAngleX)
      return;

    int delta = targetAngleX - currentAngleX;
    int step = constrain(abs(delta) / 4 + 1, 1, 8);
    currentAngleX += (delta > 0) ? step : -step;

    if ((delta > 0 && currentAngleX > targetAngleX) ||
        (delta < 0 && currentAngleX < targetAngleX))
      currentAngleX = targetAngleX;

    servoX.write(currentAngleX);
  }

  void tickAutoStop() {
    const uint64_t AUTOSTOP_TIMEOUT_MS = 500;
    uint64_t now = nowMs();

    int64_t diff = elapsedSince(lastCommandTime);

    if (!motorStopped && (uint64_t)diff > AUTOSTOP_TIMEOUT_MS) {
      stop();
      Serial.println("[AutoStop] No command for 500ms, stopping motors");
    }
  }

  esp_err_t initCamera() {
    esp_err_t err = esp_camera_init(&camera_config);

    if (err != ESP_OK) {
      Serial.printf("Camera error: 0x%x\n", err);
      return err;
    }

    sensor_t *s = esp_camera_sensor_get();
    if (!s) {
      Serial.println("NO SENSOR DETECTED");
      return ESP_FAIL;
    }

    s->set_framesize(s, FRAMESIZE_VGA);
    delay(100);

    Serial.println("Camera initialized");
    return ESP_OK;
  }

  void initMotors() {
    motorL.setMinPwm(200);
    motorR.setMinPwm(200);

    motorL.begin();
    motorR.begin();
  }
};

#endif // CAR_H
