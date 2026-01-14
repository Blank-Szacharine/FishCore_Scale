#include "scale.h"
#include <Wire.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>
#include "config.h"

static NAU7802 g_scale;
static TwoWire g_scaleWire(1);

bool ScaleManager::begin() {
  g_scaleWire.begin(SCALE_SDA_PIN, SCALE_SCL_PIN);

  if (!g_scale.begin(g_scaleWire)) {
    initialized_ = false;
    return false;
  }

  g_scale.setGain(NAU7802_GAIN_128);
  g_scale.setSampleRate(NAU7802_SPS_10); // lower SPS for better stability
  g_scale.setChannel(NAU7802_CHANNEL_1);
  g_scale.calibrateAFE();

  // Allow AFE to settle and discard early conversions
  delay(2000);
  for (int i = 0; i < 16; ++i) {
    (void)g_scale.getReading();
    delay(8);
  }

  // Apply default calibration factor upfront so any derived checks use correct scaling
  calFactor_ = SCALE_CAL_FACTOR_DEFAULT;
  g_scale.setCalibrationFactor(calFactor_);

  // Tare on startup to ensure zeroed weight (multiple attempts if needed)
  const uint16_t tareSamples = 48;  // ~5s at 10SPS
  const float verifyGrams = 80.0f;  // accept +/-80g residual
  const int maxAttempts = 4;

  for (int attempt = 0; attempt < maxAttempts; ++attempt) {
    g_scale.calculateZeroOffset(tareSamples);
    delay(100);

    // Verify using raw counts relative to zero offset to avoid unit mismatches
    long z = g_scale.getZeroOffset();
    long sum = 0;
    const uint8_t verifyN = 16;
    for (uint8_t i = 0; i < verifyN; ++i) {
      sum += g_scale.getReading();
      delay(6);
    }
    long avgCounts = sum / verifyN;
    long diffCounts = labs(avgCounts - z);

    long thresholdCounts = (long)(calFactor_ * verifyGrams); // counts corresponding to verifyGrams
    if (diffCounts <= thresholdCounts) {
      break;
    }
  }
  zeroOffset_ = g_scale.getZeroOffset();
  g_scale.setZeroOffset(zeroOffset_);

  initialized_ = true;
  return true;
}

void ScaleManager::tare(uint16_t samples) {
  if (!initialized_) return;
  g_scale.calculateZeroOffset(samples);
  zeroOffset_ = g_scale.getZeroOffset();
  g_scale.setZeroOffset(zeroOffset_);
}

float ScaleManager::getWeightKg(bool averaged) {
  if (!initialized_) return 0.0f;
  const uint8_t samples = averaged ? 8 : 1;
  float sumGrams = 0.0f;
  for (uint8_t i = 0; i < samples; ++i) {
    sumGrams += g_scale.getWeight(true); // returns grams using current cal factor
    delay(2);
  }
  float grams = sumGrams / samples;
  float kg = grams / 1000.0f;
  if (kg < 0.0f) kg = -kg; // ensure positive orientation for display
  if (fabsf(kg) < 0.002f) kg = 0.0f; // deadband ~2g
  return kg;
}
