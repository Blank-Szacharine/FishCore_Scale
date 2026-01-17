#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "config.h"

// Minimal driver for M5Stack UNIT RFID2 (WS1850S) over I2C.
// Note: Replace readUid_() with the proper WS1850S command frame per datasheet.

class RFID2 {
 public:
  RFID2();

  bool begin(TwoWire &wire, uint8_t addr = RFID2_ADDR_DEFAULT);
  bool isConnected() const;
  bool poll(String &id);
  const String &lastId() const;

  // New lightweight accessors
  inline bool ready() const { return connected_; }
  inline uint8_t address() const { return addr_; }

 private:
  TwoWire *wire_ = nullptr;
  uint8_t addr_ = 0x00;
  bool connected_ = false;
  String lastId_;

  bool probe_(uint8_t a);
  bool readUid_(String &out);
};
