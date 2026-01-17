#include "rfid2.h"
#include <MFRC522_I2C.h>

// Single instance for the MFRC522 reader
static MFRC522_I2C* mfrc522_ = nullptr;

RFID2::RFID2() = default;

bool RFID2::begin(TwoWire &wire, uint8_t addr) {
  wire_ = &wire;
  addr_ = addr;
  // Share the same bus as LCD; make sure Wire is already begun.
  connected_ = probe_(addr_);
  if (!connected_) {
    // Try a fallback address if needed
    connected_ = probe_(RFID2_ADDR_FALLBACK);
    if (connected_) addr_ = RFID2_ADDR_FALLBACK;
  }
  if (connected_) {
    // Initialize MFRC522_I2C at the detected address
    if (mfrc522_) {
      delete mfrc522_;
      mfrc522_ = nullptr;
    }
    // Pass TwoWire by reference as first arg, then I2C address
    mfrc522_ = new MFRC522_I2C(addr_, RFID_RST_PIN);
    mfrc522_->PCD_Init();

  }
  return connected_;
}

bool RFID2::isConnected() const { return connected_; }

// Poll for a tag. Returns true when a new/non-empty ID is read.
bool RFID2::poll(String &id) {
  if (!connected_ || wire_ == nullptr) return false;
  String newId;
  if (!readUid_(newId)) return false;
  if (newId.length() == 0) return false;
  if (newId != lastId_) {
    lastId_ = newId;
    id = lastId_;
    return true;
  }
  id = lastId_;
  return false;
}

const String &RFID2::lastId() const { return lastId_; }

bool RFID2::probe_(uint8_t a) {
  wire_->beginTransmission(a);
  return (wire_->endTransmission() == 0);
}

bool RFID2::readUid_(String &out) {
  if (!connected_ || wire_ == nullptr || mfrc522_ == nullptr) {
    out = "";
    return false;
  }

  // Use MFRC522_I2C API to detect and read a new card UID
  if (!mfrc522_->PICC_IsNewCardPresent()) {
    out = "";
    return false;
  }
  if (!mfrc522_->PICC_ReadCardSerial()) {
    out = "";
    return false;
  }

  // Build hex UID string
  String hexStr;
  hexStr.reserve(mfrc522_->uid.size * 2);
  for (uint8_t k = 0; k < mfrc522_->uid.size; ++k) {
    char buf[3];
    snprintf(buf, sizeof(buf), "%02X", mfrc522_->uid.uidByte[k]);
    hexStr += buf;
  }
  out = hexStr;

  // Halt the card and stop crypto to be ready for the next read
  mfrc522_->PICC_HaltA();
  mfrc522_->PCD_StopCrypto1();
  return true;
}