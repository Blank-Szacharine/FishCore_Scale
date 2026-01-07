#pragma once
#include <Arduino.h>

class ADS1232 {
 public:
  ADS1232(uint8_t doutPin, uint8_t sclkPin);
  bool begin();
  // Checks if ADC is responding by waiting for DRDY low within timeout
  bool isAvailable(uint32_t timeoutMs);
  // Read one raw sample (blocking until ready or timeout). Returns true on success.
  bool readRaw(int32_t &value, uint32_t timeoutMs);
  // Take multiple samples to compute zero offset
  bool tare(uint16_t samples = 16);
  // Convert last raw reading to weight using calibration factor
  float toWeight(int32_t raw) const;
  void setCalibration(float factor) { calibration_ = factor; }
  float calibration() const { return calibration_; }
  int32_t offset() const { return offset_; }

 private:
  uint8_t dout_;
  uint8_t sclk_;
  float calibration_ = 1.0f;
  int32_t offset_ = 0;
  void clockPulse();
};
