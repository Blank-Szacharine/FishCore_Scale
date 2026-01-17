// ESP32 + NAU7802 load cell with 20x4 I2C LCD
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include "config.h"
#include "modules/lcd_display.h"
#include "modules/scale.h"
#include "modules/rfid2.h"

// LCD on default I2C (pins from config.h)
LCDDisplay lcd;
bool lcdOK = false;
uint8_t lcdAddr = 0x00;

// Scale manager encapsulates NAU7802 logic
ScaleManager scale;

// RFID2 on same I2C bus as LCD
RFID2 rfid;
bool rfidOK = false;

// Track when the scale finished zeroing successfully
bool scaleReady = false;

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

bool initScale() {
  if (!scale.begin()) {
    lcdStatus("LOAD CELL NOT FOUND", "Check wiring & power");
    return false;
  }
  // Calibration completed successfully
  return true;
}

void setup() {
  Serial.begin(115200);
  delay(150);

  // Init I2C first, slower for compatibility
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(50);

  // Initialize LCD
  lcdOK = lcd.begin(Wire, lcdAddr);
  if (lcdOK) {
    lcd.printLine(0, "Calibrating");
    if (LCD_ROWS > 1) lcd.printLine(1, "Please wait...");
  } else {
    Serial.println("LCD FAIL");
  }

  // Initialize RFID2 and show readiness on line 2 (row 1)
  delay(30);
  rfidOK = rfid.begin(Wire);
  if (lcdOK && LCD_ROWS > 1) {
    lcd.printLine(1, rfidOK ? "RFID: Ready" : "RFID: Not found");
  }

  // Initialize scale (module tares automatically)
  if (!initScale()) return; // message shown already
  if (lcdOK && LCD_ROWS > 1) lcd.printLine(1, ""); // clear readiness line for future ID

  // Force zero before the first weight is shown, retry if needed
  if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, "Zeroing...");
  int attempts = 0;
  do {
    scale.tare(128);
    delay(50);
  } while (fabsf(scale.getWeightKg(true)) > 0.02f && ++attempts < 3);

  // Confirm zero and set ready flag
  float zeroKg = scale.getWeightKg(true);
  scaleReady = (fabsf(zeroKg) <= 0.02f);
  if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, scaleReady ? "Ready             " : "Zero failed       ");
  // After ready, set ID label on row 1 and clear row 2
  if (scaleReady) {
    if (lcdOK && LCD_ROWS > 1) lcd.printLine(1, "ID:");
    if (lcdOK && LCD_ROWS > 2) lcd.printLine(2, "                    ");
  }
  if (!scaleReady) Serial.println("Zeroing did not reach threshold");
}

void loop() {
  if (!lcdOK) {
    delay(500);
    return;
  }

  // Read and display weight (kg only) on row 0
  float kg = scale.getWeightKg(true);
  char l0[21];
  snprintf(l0, sizeof(l0), "Weight %6.2f kg", kg);
  lcd.printLine(0, String(l0));

  // RFID: after zeroing, keep "ID:" on row 1 and update row 2 with value
  if (scaleReady) {
    if (lcdOK && LCD_ROWS > 1) lcd.printLine(1, "ID:");
    if (rfidOK) {
      String id;
      bool changed = rfid.poll(id);
      if (changed) {
        if (id.length()) {
          if (lcdOK && LCD_ROWS > 2) lcd.printLine(2, id);
        } else {
          if (lcdOK && LCD_ROWS > 2) lcd.printLine(2, "                    "); // clear when tag removed
        }
      }
    } else {
      // Retry RFID init periodically (no LCD spam; label remains)
      static unsigned long lastRetry = 0;
      if (millis() - lastRetry > 1000) {
        lastRetry = millis();
        rfidOK = rfid.begin(Wire);
        if (rfidOK) Serial.println("RFID reinitialized");
      }
    }
  }
  // Optional serial control: 't' to re-tare
  if (Serial.available()) {
    char cmd = (char)Serial.read();
    if (cmd == 't' || cmd == 'T') {
      lcd.printLine(3, "Tare...                ");
      scale.tare(64);
      float zeroKg2 = scale.getWeightKg(true);
      scaleReady = (fabsf(zeroKg2) <= 0.02f);
      lcd.printLine(3, scaleReady ? "Tare done               " : "Tare not zero            ");
      // Restore ID label/value area after tare
      if (scaleReady) {
        if (lcdOK && LCD_ROWS > 1) lcd.printLine(1, "ID:");
        if (lcdOK && LCD_ROWS > 2) lcd.printLine(2, "                    ");
      }
    }
  }

  delay(250);
}
