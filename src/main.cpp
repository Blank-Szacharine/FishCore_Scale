// ESP32 + NAU7802 load cell with 20x4 I2C LCD
#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "modules/lcd_display.h"
#include "modules/scale.h"

// LCD on default I2C (pins from config.h)
LCDDisplay lcd;
bool lcdOK = false;
uint8_t lcdAddr = 0x00;

// Scale manager encapsulates NAU7802 logic
ScaleManager scale;

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

  // Initialize LCD and show basic status
  lcdOK = lcd.begin(Wire, lcdAddr);
  if (lcdOK) {
    lcd.printLine(0, "Weighing Scale Calibrating");
    if (LCD_ROWS > 1) lcd.printLine(1, "Please wait...");
  } else {
    Serial.println("LCD FAIL");
  }

  // Initialize scale (module tares automatically)
  if (!initScale()) return; // message shown already
  if (lcdOK && LCD_ROWS > 1) lcd.printLine(1, ""); // clear the hint line
}

void loop() {
  if (!lcdOK) {
    delay(500);
    return;
  }

  // Read and display weight (kg only)
  float kg = scale.getWeightKg(true);
  char l0[21];
  snprintf(l0, sizeof(l0), "Weight %6.2f kg", kg);
  lcd.printLine(0, String(l0));

  // Optional serial control: 't' to re-tare (USB not required for normal use)
  if (Serial.available()) {
    char cmd = (char)Serial.read();
    if (cmd == 't' || cmd == 'T') {
      lcd.printLine(3, "Tare...                ");
      scale.tare(64);
      lcd.printLine(3, "Tare done               ");
    }
  }

  delay(250);
}
