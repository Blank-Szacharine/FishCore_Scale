#pragma once
#include <Arduino.h>

class ScaleManager {
 public:
  bool begin();
  void tare(uint16_t samples = 64);
  float getWeightKg(bool averaged = true);

  long zeroOffset() const { return zeroOffset_; }
  float calFactor() const { return calFactor_; }

 private:
  long zeroOffset_ = 0;
  float calFactor_ = 0.0f;
  bool initialized_ = false;

  // Checks which direction is positive and flips calibration sign if needed.
  void autoFixDirection();
};
