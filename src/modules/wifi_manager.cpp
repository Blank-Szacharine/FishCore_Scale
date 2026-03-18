#include "wifi_manager.h"

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

namespace {
constexpr const char *kPrefsNamespace = "wifi";
constexpr const char *kKeySsid = "ssid";
constexpr const char *kKeyPass = "pass";

WebServer *asServer(void *p) { return reinterpret_cast<WebServer *>(p); }

WiFiManager::Credentials normalizeCreds(const WiFiManager::Credentials &in) {
  WiFiManager::Credentials out = in;
  out.ssid.trim();
  // password may intentionally contain spaces; do not trim.
  return out;
}
} // namespace

WiFiManager::Credentials WiFiManager::loadSavedCredentials() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, true)) {
    return {};
  }

  Credentials creds;
  creds.ssid = prefs.getString(kKeySsid, "");
  creds.password = prefs.getString(kKeyPass, "");
  prefs.end();
  return normalizeCreds(creds);
}

bool WiFiManager::saveCredentials(const Credentials &credsIn) {
  Credentials creds = normalizeCreds(credsIn);
  if (!creds.valid()) return false;

  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return false;
  }

  bool ok1 = prefs.putString(kKeySsid, creds.ssid) > 0;
  bool ok2 = prefs.putString(kKeyPass, creds.password) >= 0;
  prefs.end();
  return ok1 && ok2;
}

void WiFiManager::clearSavedCredentials() {
  Preferences prefs;
  if (!prefs.begin(kPrefsNamespace, false)) {
    return;
  }
  prefs.remove(kKeySsid);
  prefs.remove(kKeyPass);
  prefs.end();
}

bool WiFiManager::isConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::connectSta_(const Credentials &creds, uint32_t timeoutMs) {
  if (!creds.valid()) return false;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(creds.ssid.c_str(), creds.password.c_str());

  const uint32_t startMs = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < timeoutMs) {
    delay(150);
  }

  return WiFi.status() == WL_CONNECTED;
}

bool WiFiManager::ensureConnected(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) return true;
  if (configPortalActive_) return false;

  Credentials saved = loadSavedCredentials();
  if (!saved.valid()) return false;

  Serial.printf("Reconnecting WiFi SSID: %s\n", saved.ssid.c_str());
  return connectSta_(saved, timeoutMs);
}

String WiFiManager::makeApSsid_() {
  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char buf[64];
  snprintf(buf, sizeof(buf), "FishCore-Scale-Setup-%02X%02X%02X", mac[3], mac[4], mac[5]);
  return String(buf);
}

String WiFiManager::htmlEscape_(const String &in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); ++i) {
    const char c = in[i];
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c; break;
    }
  }
  return out;
}

void WiFiManager::startConfigPortal_() {
  configPortalActive_ = true;

  apSsid_ = makeApSsid_();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid_.c_str());
  apIp_ = WiFi.softAPIP().toString();

  if (server_ == nullptr) {
    server_ = new WebServer(80);
  }
  WebServer &server = *asServer(server_);

  server.on("/", HTTP_GET, [this]() {
    Credentials saved = loadSavedCredentials();
    const String savedSsid = htmlEscape_(saved.ssid);

    String html;
    html.reserve(1500);
    html += "<!doctype html><html><head><meta charset='utf-8'>";
    html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<title>FishCore WiFi Setup</title></head><body>";
    html += "<h2>Configure WiFi</h2>";
    html += "<form method='POST' action='/save'>";
    html += "<label>SSID</label><br>";
    html += "<input name='ssid' maxlength='32' value='" + savedSsid + "' style='width: 100%; max-width: 320px;' required><br><br>";
    html += "<label>Password</label><br>";
    html += "<input name='pass' type='password' maxlength='64' style='width: 100%; max-width: 320px;'><br><br>";
    html += "<button type='submit'>Save & Restart</button>";
    html += "</form>";
    html += "<p style='margin-top:16px;'><a href='/clear'>Clear saved WiFi credentials</a></p>";
    html += "</body></html>";

    asServer(server_)->send(200, "text/html", html);
  });

  server.on("/save", HTTP_POST, [this]() {
    const String ssid = asServer(server_)->arg("ssid");
    const String pass = asServer(server_)->arg("pass");

    Credentials creds;
    creds.ssid = ssid;
    creds.password = pass;

    if (!saveCredentials(creds)) {
      asServer(server_)->send(400, "text/plain", "Invalid SSID. Nothing saved.");
      return;
    }

    asServer(server_)->send(200, "text/plain", "Saved. Rebooting...");
    delay(500);
    ESP.restart();
  });

  server.on("/clear", HTTP_GET, [this]() {
    clearSavedCredentials();
    asServer(server_)->send(200, "text/plain", "Cleared saved WiFi credentials. Rebooting...");
    delay(500);
    ESP.restart();
  });

  server.onNotFound([this]() { asServer(server_)->send(404, "text/plain", "Not found"); });

  server.begin();

  Serial.println("WiFi config portal started");
  Serial.printf("AP SSID: %s\n", apSsid_.c_str());
  Serial.printf("AP IP:   %s\n", apIp_.c_str());
}

bool WiFiManager::begin(uint32_t connectTimeoutMs) {
  return begin(connectTimeoutMs, Credentials{});
}

bool WiFiManager::begin(uint32_t connectTimeoutMs, const Credentials &fallbackIn) {
  configPortalActive_ = false;

  // 1) Try saved credentials
  Credentials saved = loadSavedCredentials();
  if (saved.valid()) {
    Serial.printf("Connecting WiFi (saved) SSID: %s\n", saved.ssid.c_str());
    if (connectSta_(saved, connectTimeoutMs)) {
      Serial.printf("WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }
  } else {
    Serial.println("No saved WiFi credentials in NVS");
  }

  // 2) Optional fallback credentials (e.g., compile-time defaults)
  Credentials fallback = normalizeCreds(fallbackIn);
  if (fallback.valid()) {
    Serial.printf("Connecting WiFi (fallback) SSID: %s\n", fallback.ssid.c_str());
    if (connectSta_(fallback, connectTimeoutMs)) {
      Serial.printf("WiFi connected. IP: %s\n", WiFi.localIP().toString().c_str());
      return true;
    }
  }

  // 3) Failed -> enter AP config mode
  Serial.println("WiFi connect failed; starting AP config portal");
  startConfigPortal_();
  return false;
}

void WiFiManager::loop() {
  if (!configPortalActive_ || server_ == nullptr) return;
  asServer(server_)->handleClient();
}
