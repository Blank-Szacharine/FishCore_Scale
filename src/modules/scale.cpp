#include "scale.h"
#include <Wire.h>
#include <math.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>
#include "config.h"

static NAU7802 g_scale;
static TwoWire g_scaleWire(1);

bool ScaleManager::begin() {
  // Start NAU7802 on its own I2C bus (GPIO16/17 from config.h)
  g_scaleWire.begin(SCALE_SDA_PIN, SCALE_SCL_PIN);

  if (!g_scale.begin(g_scaleWire)) {
    initialized_ = false;
    return false;
  }

  // Basic NAU config
  g_scale.setGain(NAU7802_GAIN_128);
  g_scale.setSampleRate(NAU7802_SPS_20);   // faster updates, still stable
  g_scale.setChannel(NAU7802_CHANNEL_1);

  // Calibrate analog front-end
  if (!g_scale.calibrateAFE()) {
    initialized_ = false;
    return false;
  }

  // Let it settle and discard early conversions (shortened)
  delay(400);
  for (int i = 0; i < 6; i++) {
    (void)g_scale.getReading();
    delay(5);
  }

  // Set a calibration factor (grams scaling). We'll flip sign if needed.
  calFactor_ = SCALE_CAL_FACTOR_DEFAULT;
  g_scale.setCalibrationFactor(calFactor_);

  // Shorter tare at boot
  tare(24);

  // Fix direction if putting weight makes reading negative
  autoFixDirection();

  // Small final tare after any direction change
  tare(8);

  initialized_ = true;
  return true;
}

void ScaleManager::tare(uint16_t samples) {
  if (!g_scale.isConnected()) return;

  // At 10SPS, 128 samples ~12.8s. Give it enough time.
  const uint32_t timeoutMs = (uint32_t)samples * 120 + 500; // ~100ms/sample + margin

  g_scale.calculateZeroOffset(samples, timeoutMs);
  zeroOffset_ = g_scale.getZeroOffset();
  g_scale.setZeroOffset(zeroOffset_);
}


void ScaleManager::autoFixDirection() {
  // We detect direction by checking raw delta from the ADC (independent of calibration).
  // Ask user to do nothing; we just observe noise & sign. If it’s inverted due to wiring,
  // the library-weight might go negative when load increases. We'll fix by flipping calFactor.

  // Grab a baseline raw average
  long base = 0;
  const int n = 16;
  for (int i = 0; i < n; i++) {
    base += g_scale.getReading();
    delay(10);
  }
  base /= n;

  // Wait a moment and read again. If signal drifts the other way, that's OK—this is just a sanity check.
  long later = 0;
  for (int i = 0; i < n; i++) {
    later += g_scale.getReading();
    delay(10);
  }
  later /= n;

  // If your system is wired such that adding weight results in negative grams,
  // the simplest robust fix is: flip calibration sign.
  //
  // Since we can’t force you to add a known load at boot, we instead do a practical check:
  // If current computed weight is negative (beyond a small noise band), flip sign.
  float gramsNow = g_scale.getWeight(true); // uses current zeroOffset + calFactor
  if (gramsNow < -5.0f) {                   // more than -5g = likely inverted
    calFactor_ = -fabsf(calFactor_);
    g_scale.setCalibrationFactor(calFactor_);
  }

  // (Optional) If you want to log raw drift:
  (void)base;
  (void)later;
}

float ScaleManager::getWeightKg(bool averaged) {
  if (!initialized_) return 0.0f;

  // Fewer samples and no extra delay: rely on the NAU7802's own
  // filtering plus the higher-level stability buffer in main.cpp.
  const uint8_t samples = averaged ? 4 : 1;
  float sumGrams = 0.0f;

  for (uint8_t i = 0; i < samples; i++) {
    sumGrams += g_scale.getWeight(true); // grams
  }

  float grams = sumGrams / samples;
  float kg = grams / 1000.0f;

  // deadband near zero
  if (fabsf(kg) < 0.002f) kg = 0.0f;

  // Ensure reported weight is never negative
  if (kg < 0.0f) kg = -kg;

  return kg;
}
