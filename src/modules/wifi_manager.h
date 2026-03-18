#pragma once

#include <Arduino.h>

// Handles:
// - Loading/saving Wi-Fi credentials via Preferences (NVS)
// - Connecting to Wi-Fi in STA mode on boot
// - Falling back to AP mode with a small config web page
// - Clearing credentials on request
//
// This module is intentionally independent of the scale / LCD / RFID logic.
class WiFiManager {
 public:
  struct Credentials {
    String ssid;
    String password;
    bool valid() const { return ssid.length() > 0; }
  };

  // Call once from setup().
  // Returns true if connected to Wi-Fi (STA). Returns false if it entered AP config mode.
  bool begin(uint32_t connectTimeoutMs);

  // Same as begin(), but if saved credentials fail (or don't exist), also tries
  // the provided fallback credentials before starting AP config mode.
  bool begin(uint32_t connectTimeoutMs, const Credentials &fallback);

  // Call from loop() to process HTTP requests while in AP mode.
  void loop();

  // Returns true when STA is connected.
  bool isConnected() const;

  // Returns true when AP config portal is running.
  bool isConfigPortalActive() const { return configPortalActive_; }

  // Ensure Wi-Fi is connected in STA mode.
  // Attempts a reconnect using saved credentials (does NOT start AP mode).
  bool ensureConnected(uint32_t timeoutMs);

  // Clears saved credentials from NVS.
  void clearSavedCredentials();

  // Returns current AP SSID (only valid if AP is active).
  String apSsid() const { return apSsid_; }

  // Returns current AP IP address as string (only valid if AP is active).
  String apIp() const { return apIp_; }

  // Load saved credentials (if any). Returns {"",""} if none.
  Credentials loadSavedCredentials();

  // Save credentials to NVS.
  bool saveCredentials(const Credentials &creds);

 private:
  bool connectSta_(const Credentials &creds, uint32_t timeoutMs);
  void startConfigPortal_();

  static String makeApSsid_();
  static String htmlEscape_(const String &in);

  bool configPortalActive_ = false;
  String apSsid_;
  String apIp_;

  // Lazy-created in .cpp (to avoid exposing WebServer header in other files).
  void *server_ = nullptr;
};
