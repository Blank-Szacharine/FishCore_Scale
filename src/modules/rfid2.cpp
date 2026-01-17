#include "rfid2.h"

// Share the same bus as LCD; make sure Wire is already begun.
bool RFID2::begin(TwoWire &wire, uint8_t addr) {
  wire_ = &wire;
  addr_ = addr;
  connected_ = probe_(addr_);
  if (!connected_) {
    connected_ = probe_(RFID2_ADDR_FALLBACK);
    if (connected_) addr_ = RFID2_ADDR_FALLBACK;
  }
  return connected_;
}

bool RFID2::isConnected() const {
  return connected_;
}

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

const String &RFID2::lastId() const {
  return lastId_;
}

bool RFID2::probe_(uint8_t a) {
  wire_->beginTransmission(a);
  return (wire_->endTransmission() == 0);
}

bool RFID2::readUid_(String &out) {
  // TODO: Replace this with WS1850S-specific command to fetch UID.
  // Placeholder strategy: try to read a status/UID buffer.
  // If your module requires a command, write the command frame before requesting.
  // Example (pseudo):
  // wire_->beginTransmission(addr_);
  // wire_->write(0x01); // CMD_GET_UID (replace per datasheet)
  // wire_->endTransmission();
  // delay(5);

  const uint8_t maxBytes = 16;
  int n = wire_->requestFrom((int)addr_, (int)maxBytes);
  if (n <= 0) {
    out = "";
    return false;
  }

  uint8_t buf[maxBytes];
  int i = 0;
  while (wire_->available() && i < maxBytes) {
    buf[i++] = wire_->read();
  }

  int start = -1, end = -1;
  for (int k = 0; k < i; ++k) {
    if (buf[k] != 0 && start < 0) start = k;
    if (start >= 0 && buf[k] != 0) end = k;
  }
  if (start < 0 || end < start) {
    out = "";
    return false;
  }
  int uidLen = end - start + 1;
  if (uidLen < 4 || uidLen > 10) {
    out = "";
    return false;
  }

  char hex[(uidLen * 2) + 1];
  for (int k = 0; k < uidLen; ++k) {
    snprintf(&hex[k * 2], 3, "%02X", buf[start + k]);
  }
  out = String(hex);
  return true;
}