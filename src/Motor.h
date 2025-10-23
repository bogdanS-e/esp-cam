class Motor {
public:
  Motor(int pinIN1, int pinIN2, int pwmChannel1, int pwmChannel2, int pwmFreq = 1000, int pwmResolution = 8)
      : _pinIN1(pinIN1), _pinIN2(pinIN2),
        _pwmChannel1(pwmChannel1), _pwmChannel2(pwmChannel2), 
        _pwmFreq(pwmFreq), _pwmResolution(pwmResolution) {}

  void begin() {
    pinMode(_pinIN1, OUTPUT);
    pinMode(_pinIN2, OUTPUT);
    
    ledcSetup(_pwmChannel1, _pwmFreq, _pwmResolution);
    ledcSetup(_pwmChannel2, _pwmFreq, _pwmResolution);
    
    ledcAttachPin(_pinIN1, _pwmChannel1);
    ledcAttachPin(_pinIN2, _pwmChannel2);
    
    stop();
  }

  void moveForward(uint8_t speed) {
    ledcWrite(_pwmChannel1, speed);
    ledcWrite(_pwmChannel2, 0);
  }

  void moveBackward(uint8_t speed) {
    ledcWrite(_pwmChannel1, 0);
    ledcWrite(_pwmChannel2, speed);
  }

  void stop() {
    ledcWrite(_pwmChannel1, 0);
    ledcWrite(_pwmChannel2, 0);
  }

private:
  int _pinIN1, _pinIN2;
  int _pwmChannel1, _pwmChannel2;
  int _pwmFreq;
  int _pwmResolution;
};