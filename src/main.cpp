// ESP32 + NAU7802 load cell with 20x4 I2C LCD
#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <SparkFun_Qwiic_Scale_NAU7802_Arduino_Library.h>
#include "config.h"
#include "modules/lcd_display.h"

// LCD on default I2C (pins from config.h)
LCDDisplay lcd;
bool lcdOK = false;
uint8_t lcdAddr = 0x00;

// NAU7802 on second I2C bus
TwoWire ScaleWire(1);
NAU7802 scale;

Preferences prefs;
static const char *PREF_NS = "scale";
static const char *KEY_OK = "cal_ok";
static const char *KEY_ZERO = "zero_off";
static const char *KEY_FACT = "cal_fact";

long zeroOffset = 0;
float calFactor = 1.0f; // grams per count

static void showCentered(uint8_t row, const String &text) {
  // Simple helper to center small messages on the LCD
  String t = text;
  if (t.length() < LCD_COLS) {
    int pad = (LCD_COLS - t.length()) / 2;
    String spaces = String(' ', pad);
    t = spaces + t;
  }
  lcd.printLine(row, t);
}

static void lcdStatus(const String &l0, const String &l1 = "", const String &l2 = "", const String &l3 = "") {
  if (!lcdOK) return;
  lcd.printLine(0, l0);
  if (LCD_ROWS > 1) lcd.printLine(1, l1);
  if (LCD_ROWS > 2) lcd.printLine(2, l2);
  if (LCD_ROWS > 3) lcd.printLine(3, l3);
}

bool loadCalibrationFromNVS() {
  prefs.begin(PREF_NS, true);
  bool ok = prefs.getBool(KEY_OK, false);
  if (ok) {
    zeroOffset = prefs.getLong(KEY_ZERO, 0);
    calFactor = prefs.getFloat(KEY_FACT, 1.0f);
  }
  prefs.end();
  return ok;
}

void saveCalibrationToNVS(long zero, float factor) {
  prefs.begin(PREF_NS, false);
  prefs.putLong(KEY_ZERO, zero);
  prefs.putFloat(KEY_FACT, factor);
  prefs.putBool(KEY_OK, true);
  prefs.end();
}

bool initScale() {
  ScaleWire.begin(SCALE_SDA_PIN, SCALE_SCL_PIN);

  if (!scale.begin(ScaleWire)) {
    Serial.println("NAU7802 not detected");
    lcdStatus("LOAD CELL NOT FOUND", "Check wiring & power");
    return false;
  }

  // Basic configuration for stability
  scale.setGain(NAU7802_GAIN_128);
  scale.setSampleRate(NAU7802_SPS_40);
  scale.setChannel(NAU7802_CHANNEL_1);
  scale.calibrateAFE();
  lcdStatus("LOAD CELL Present", "NAU7802 ready");
  delay(800);
  return true;
}

void runTareAndApplyDefaultFactor() {
  lcdStatus("LOAD CELL CALIBRATING", "Remove all weight", "Taring in 3...");
  delay(1000);
  lcd.printLine(2, "Taring in 2...");
  delay(1000);
  lcd.printLine(2, "Taring in 1...");
  delay(1000);
  lcd.printLine(2, "Taring...       ");

  scale.calculateZeroOffset(64);
  zeroOffset = scale.getZeroOffset();

  // Apply default calibration factor from config
  calFactor = SCALE_CAL_FACTOR_DEFAULT;
  scale.setZeroOffset(zeroOffset);
  scale.setCalibrationFactor(calFactor);

  saveCalibrationToNVS(zeroOffset, calFactor);
  lcdStatus("LOAD CELL Present", "Calibration saved", "Ready to weigh");
  delay(1200);
}

void setup() {
  Serial.begin(115200);
  delay(150);

  // Initialize LCD and show basic status
  lcdOK = lcd.begin(Wire, lcdAddr);
  if (lcdOK) {
    lcd.printLine(0, "FishCore Scale");
    char buf[21];
    snprintf(buf, sizeof(buf), "LCD: 0x%02X", lcdAddr);
    lcd.printLine(1, String(buf));
  } else {
    Serial.println("LCD FAIL");
  }

  // Initialize scale
  if (!initScale()) return; // show message already

  // Load or perform calibration
  bool hasCal = loadCalibrationFromNVS();
  if (!hasCal) {
    runTareAndApplyDefaultFactor();
  } else {
    scale.setZeroOffset(zeroOffset);
    scale.setCalibrationFactor(calFactor);
    lcdStatus("LOAD CELL Present", "Calibration loaded");
    delay(800);
  }
}

void loop() {
  if (!lcdOK) {
    delay(500);
    return;
  }

  // Read and display weight
  float grams = scale.getWeight(true); // averaged

  // Show as kilograms + grams remainder on line 0
  long rounded = (long)(grams + (grams >= 0.0f ? 0.5f : -0.5f));
  long kg = rounded / 1000L;
  long remG = labs(rounded - kg * 1000L);

  char l0[21];
  snprintf(l0, sizeof(l0), "Wt: %ldkg %3ldg", kg, remG);
  lcd.printLine(0, String(l0));

  // Also show exact grams on line 1
  char l1[21];
  snprintf(l1, sizeof(l1), "= %10.2f g", grams);
  lcd.printLine(1, String(l1));

  // Status lines
  char l2[21];
  snprintf(l2, sizeof(l2), "Zero:%9ld", zeroOffset);
  lcd.printLine(2, String(l2));

  char l3[21];
  snprintf(l3, sizeof(l3), "Cal:%10.6f", calFactor);
  lcd.printLine(3, String(l3));

  // Optional serial control: 't' to re-tare (USB not required for normal use)
  if (Serial.available()) {
    char cmd = (char)Serial.read();
    if (cmd == 't' || cmd == 'T') {
      lcd.printLine(3, "Tare...                ");
      scale.calculateZeroOffset(64);
      zeroOffset = scale.getZeroOffset();
      scale.setZeroOffset(zeroOffset);
      saveCalibrationToNVS(zeroOffset, calFactor);
      lcd.printLine(3, "Tare done               ");
    }
  }

  delay(250);
}
