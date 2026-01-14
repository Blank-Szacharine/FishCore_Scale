#pragma once
#include <Arduino.h>

class ScaleManager {
 public:
  // Initialize NAU7802 on its dedicated I2C pins and tare.
  bool begin();
  // Force a tare (zero) operation.
  void tare(uint16_t samples = 64);
  // Read weight in kilograms; uses current calibration factor.
  float getWeightKg(bool averaged = true);

  long zeroOffset() const { return zeroOffset_; }
  float calFactor() const { return calFactor_; }

 private:
  long zeroOffset_ = 0;
  float calFactor_ = -.006f;
  bool initialized_ = false;
};
