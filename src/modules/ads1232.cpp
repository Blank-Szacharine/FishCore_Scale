#include "ads1232.h"
#include "config.h"

ADS1232::ADS1232(uint8_t doutPin, uint8_t sclkPin) : dout_(doutPin), sclk_(sclkPin) {}

bool ADS1232::begin() {
  pinMode(dout_, INPUT);
  pinMode(sclk_, OUTPUT);
  digitalWrite(sclk_, LOW);
  calibration_ = CALIBRATION_FACTOR;
  return true;
}

bool ADS1232::isAvailable(uint32_t timeoutMs) {
  uint32_t start = millis();
  // Wait for DOUT/DRDY to go LOW indicating data ready
  while (millis() - start < timeoutMs) {
    if (digitalRead(dout_) == LOW) {
      return true;
    }
    delay(5);
  }
  return false;
}

void ADS1232::clockPulse() {
  // ADS1232 shifts out data MSB first on SCLK rising edges
  digitalWrite(sclk_, HIGH);
  delayMicroseconds(2);
  digitalWrite(sclk_, LOW);
  delayMicroseconds(2);
}

bool ADS1232::readRaw(int32_t &value, uint32_t timeoutMs) {
  // Wait for data ready
  uint32_t start = millis();
  while (digitalRead(dout_) == HIGH) {
    if (millis() - start > timeoutMs) return false;
    delay(1);
  }

  // Read 24-bit two's complement
  int32_t result = 0;
  // Sample DOUT after the SCLK falling edge (common for ADS123x)
  for (int i = 0; i < 24; i++) {
    // Rising edge: shift
    digitalWrite(sclk_, HIGH);
    delayMicroseconds(2);
    // Falling edge: data valid
    digitalWrite(sclk_, LOW);
    delayMicroseconds(2);
    result = (result << 1) | (digitalRead(dout_) & 0x01);
  }

  // One extra clock to finish the cycle/start next conversion
  clockPulse();

  // Sign extend 24-bit value to 32-bit
  if (result & 0x800000) {
    result |= 0xFF000000;
  }

  value = result;
  return true;
}

bool ADS1232::tare(uint16_t samples) {
  int64_t sum = 0;
  int32_t v = 0;
  uint16_t collected = 0;
  uint32_t timeout = ADS_READY_TIMEOUT_MS;
  for (uint16_t i = 0; i < samples; i++) {
    if (!readRaw(v, timeout)) {
      // try again for this sample
      continue;
    }
    sum += v;
    collected++;
  }
  if (collected == 0) return false;
  offset_ = static_cast<int32_t>(sum / collected);
  return true;
}

float ADS1232::toWeight(int32_t raw) const {
  // Apply offset then calibration factor
  int32_t corrected = raw - offset_;
  return static_cast<float>(corrected) * calibration_;
}
