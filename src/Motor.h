#pragma once
#include "utils.h"
#include <Arduino.h>

#define MOTOR_PWM_FREQ 1000
class Motor {
public:
  enum class Direction : uint8_t {
    STOP = 0,
    FORWARD,
    BACKWARD
  };

  Motor(int pinIN1, int pinIN2, int pwmChannel1, int pwmChannel2)
      : _pinIN1(pinIN1), _pinIN2(pinIN2),
        _pwmChannel1(pwmChannel1), _pwmChannel2(pwmChannel2),
        _minPwm(0),
        _currentSpeed(0),
        _targetSpeed(0),
        _direction(Direction::STOP),
        _accelStep(5),
        _updateInterval(30),
        _lastUpdate(0) {}

  void begin() {
    pinMode(_pinIN1, OUTPUT);
    pinMode(_pinIN2, OUTPUT);

    ledcSetup(_pwmChannel1, MOTOR_PWM_FREQ, 8);
    ledcSetup(_pwmChannel2, MOTOR_PWM_FREQ, 8);

    ledcAttachPin(_pinIN1, _pwmChannel1);
    ledcAttachPin(_pinIN2, _pwmChannel2);

    stop();
  }

  void setMinPwm(uint8_t minPwm) {
    _minPwm = constrain(minPwm, 0, 255);
  }

  void setRamp(uint8_t accelStep, uint16_t updateInterval) {
    _accelStep = constrain(accelStep, 1, 50);
    _updateInterval = constrain(updateInterval, 5, 100);
  }

  void moveForward(uint8_t targetSpeed = 255) {
    _direction = Direction::FORWARD;
    _targetSpeed = constrain(targetSpeed, _minPwm, 255);
  }

  void moveBackward(uint8_t targetSpeed = 255) {
    _direction = Direction::BACKWARD;
    _targetSpeed = constrain(targetSpeed, _minPwm, 255);
  }

  void stop() {
    _direction = Direction::STOP;
    _targetSpeed = 0;
    _currentSpeed = 0;
  }

  void tick() {
    int64_t diff = elapsedSince(_lastUpdate);

    if (diff < _updateInterval) {
      return;
    }

    _lastUpdate = nowMs();

    if (_direction == Direction::STOP) {
      ledcWrite(_pwmChannel1, 0);
      ledcWrite(_pwmChannel2, 0);
      _currentSpeed = 0;

      return;
    }

    if (_currentSpeed == 0 && _targetSpeed > 0) {
      _currentSpeed = _minPwm;
    }

    if (_currentSpeed < _targetSpeed) {
      _currentSpeed = std::min<uint8_t>(_currentSpeed + _accelStep, _targetSpeed);
    } else if (_currentSpeed > _targetSpeed) {
      _currentSpeed = std::max<uint8_t>(_currentSpeed - _accelStep, _targetSpeed);
    }

    switch (_direction) {
    case Direction::FORWARD:
      ledcWrite(_pwmChannel1, _currentSpeed);
      ledcWrite(_pwmChannel2, 0);
      Serial.printf("Motor FORWARD - Speed: %d\n", _currentSpeed);
      break;
    case Direction::BACKWARD:
      ledcWrite(_pwmChannel1, 0);
      ledcWrite(_pwmChannel2, _currentSpeed);
      Serial.printf("Motor BACKWARD - Speed: %d\n", _currentSpeed);
      break;
    default:
      break;
    }
  }

private:
  int _pinIN1, _pinIN2;
  int _pwmChannel1, _pwmChannel2;

  uint8_t _minPwm;
  uint8_t _currentSpeed;
  uint8_t _targetSpeed;
  Direction _direction;

  uint8_t _accelStep;
  uint16_t _updateInterval;
  unsigned long _lastUpdate;
};
