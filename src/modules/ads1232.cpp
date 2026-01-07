#include "lcd_display.h"
#include <Wire.h>
#include "config.h"

static bool probeI2CAddress(TwoWire &wire, uint8_t address) {
  wire.beginTransmission(address);
  return (wire.endTransmission() == 0);
}

LCDDisplay::LCDDisplay() {}

bool LCDDisplay::begin(TwoWire &wire, uint8_t &detectedAddress) {
  wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  const uint8_t candidates[] = {
    LCD_ADDR_PRIMARY, LCD_ADDR_SECONDARY,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27,
    0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F
  };

  uint8_t found = 0x00;
  for (uint8_t addr : candidates) {
    if (probeI2CAddress(wire, addr)) {
      found = addr;
      break;
    }
  }

  if (found == 0x00) {
    initialized_ = false;
    return false;
  }

  address_ = found;
  detectedAddress = found;

  lcd_ = new LiquidCrystal_PCF8574(address_);
  if (!lcd_) {
    initialized_ = false;
    return false;
  }

  lcd_->begin(LCD_COLS, LCD_ROWS);
  lcd_->setBacklight(255);
  lcd_->setCursor(0, 0);
  initialized_ = true;
  return true;
}

void LCDDisplay::clearRow(uint8_t row) {
  if (!initialized_) return;
  lcd_->setCursor(0, row);
  for (int i = 0; i < LCD_COLS; i++) lcd_->print(" ");
}

void LCDDisplay::printLine(uint8_t row, const String &text) {
  if (!initialized_) return;

  String t = text;
  if ((int)t.length() > LCD_COLS) t = t.substring(0, LCD_COLS);

  clearRow(row);
  lcd_->setCursor(0, row);
  lcd_->print(t);
}
