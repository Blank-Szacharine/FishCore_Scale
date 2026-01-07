#include "ads1232.h"
#include "config.h"

ADS1232::ADS1232(uint8_t doutPin, uint8_t sclkPin, int8_t a0Pin, int8_t pdwnPin)
    : dout_(doutPin), sclk_(sclkPin), a0_(a0Pin), pdwn_(pdwnPin) {}

bool ADS1232::begin() {
  pinMode(dout_, INPUT);
  pinMode(sclk_, OUTPUT);
  digitalWrite(sclk_, LOW);

  if (a0_ >= 0) {
    pinMode(a0_, OUTPUT);
    digitalWrite(a0_, LOW);  // default CH1
  }
  if (pdwn_ >= 0) {
    pinMode(pdwn_, OUTPUT);
    digitalWrite(pdwn_, HIGH);  // power on
  }

  calibration_ = CALIBRATION_FACTOR;
  return true;
}

void ADS1232::setChannel(uint8_t ch) {
  if (a0_ < 0) return; // strapped in hardware
  // ch=1 -> A0 low, ch=2 -> A0 high
  digitalWrite(a0_, (ch == 2) ? HIGH : LOW);
}

bool ADS1232::waitReady(uint32_t timeoutMs) {
  uint32_t start = millis();
  while (millis() - start < timeoutMs) {
    if (digitalRead(dout_) == LOW) return true;
    delay(1);
  }
  return false;
}

inline void ADS1232::sclkHigh() { digitalWrite(sclk_, HIGH); }
inline void ADS1232::sclkLow() { digitalWrite(sclk_, LOW); }

void ADS1232::clockPulse() {
  sclkHigh();
  delayMicroseconds(2);
  sclkLow();
  delayMicroseconds(2);
}

bool ADS1232::isAvailable(uint32_t timeoutMs) {
  return waitReady(timeoutMs);
}

bool ADS1232::readRaw(int32_t &value, uint32_t timeoutMs) {
  if (!waitReady(timeoutMs)) return false;

  int32_t result = 0;
  // Read 24 bits, sample DOUT after falling edge
  for (int i = 0; i < 24; i++) {
    sclkHigh();
    delayMicroseconds(2);
    sclkLow();
    delayMicroseconds(2);
    result = (result << 1) | (digitalRead(dout_) & 0x01);
  }

  // Extra clock to start next conversion
  clockPulse();

  // Sign-extend 24-bit two's complement
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
  for (uint16_t i = 0; i < samples; i++) {
    if (!readRaw(v, ADS_READY_TIMEOUT_MS)) continue;
    sum += v;
    collected++;
  }
  if (collected == 0) return false;
  offset_ = static_cast<int32_t>(sum / collected);
  return true;
}

float ADS1232::toWeight(int32_t raw) const {
  int32_t corrected = raw - offset_;
  return static_cast<float>(corrected) * calibration_;
}
