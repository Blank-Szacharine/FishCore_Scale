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
  g_scale.setSampleRate(NAU7802_SPS_40);
  g_scale.setChannel(NAU7802_CHANNEL_1);
  g_scale.calibrateAFE();

  // Tare on startup to ensure zeroed weight
  g_scale.calculateZeroOffset(64);
  zeroOffset_ = g_scale.getZeroOffset();
  g_scale.setZeroOffset(zeroOffset_);

  // Apply default calibration factor
  calFactor_ = SCALE_CAL_FACTOR_DEFAULT;
  g_scale.setCalibrationFactor(calFactor_);

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
  float grams = g_scale.getWeight(true); // averaged flag is not used by lib; true => allow negative
  return grams / 1000.0f;
}
