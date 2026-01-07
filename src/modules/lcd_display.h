#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

class LCDDisplay {
 public:
  LCDDisplay();
  // Detects LCD address, initializes LCD, returns true if OK
  bool begin(TwoWire &wire, uint8_t &detectedAddress);
  // Convenience to print a message on a given row, clears the row first.
  void printLine(uint8_t row, const String &text);
  // Returns true if LCD initialized
  bool ok() const { return initialized_; }
  uint8_t address() const { return address_; }

 private:
  bool initialized_ = false;
  uint8_t address_ = 0x00;
  LiquidCrystal_PCF8574 *lcd_ = nullptr;
  void clearRow(uint8_t row);
};
