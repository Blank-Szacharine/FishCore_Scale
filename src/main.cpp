// ESP32 + NAU7802 + 20x4 I2C LCD
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "config.h"
#include "modules/lcd_display.h"
#include "modules/scale.h"
#include "modules/rfid2.h"

// LCD over I2C
LCDDisplay lcd;
bool lcdOK = false;
uint8_t lcdAddr = 0x00;

// Load cell / NAU7802
ScaleManager scale;

// RFID2 on same I2C bus
RFID2 rfid;
bool rfidOK = false;

bool scaleReady = false;
// WiFi status
bool wifiOK = false;

// Server endpoint (GET)
static const char *UPLOAD_BASE_URL = "https://actual.fishcore.ph/uploadWeightIns";
static const int UPLOAD_ID = 1;
static const int UPLOAD_SCALE_ID = 1;

// Stability buffer
static const int kBufN = 10;
float wBuf[kBufN] = {0};
int wIdx = 0, wCnt = 0;

enum AppState { Idle, Weighing, AskId, Sending, AwaitRemoval };
AppState state = Idle;
unsigned long stableStartMs = 0;
unsigned long zeroStartMs = 0;
float stableWeightKg = 0.0f;
unsigned long weighingStartMs = 0;

static float effectiveWeight(float kg) {
  float a = fabsf(kg);
  if (a <= MIN_EFFECTIVE_WEIGHT_KG || a < DISPLAY_ZERO_CLAMP_KG) return 0.0f;
  return kg;
}

static void showCentered(uint8_t row, const String &text) {
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
  return true;
}

static float bufferStdDev() {
  if (wCnt <= 1) return 1e9f;
  float sum = 0.0f, sum2 = 0.0f;
  for (int i = 0; i < wCnt; ++i) { sum += wBuf[i]; sum2 += wBuf[i] * wBuf[i]; }
  float mean = sum / wCnt;
  float var = (sum2 / wCnt) - (mean * mean);
  return sqrtf(fmaxf(var, 0.0f));
}

static float bufferMean() {
  if (wCnt == 0) return 0.0f;
  float sum = 0.0f;
  for (int i = 0; i < wCnt; ++i) sum += wBuf[i];
  return sum / wCnt;
}

static void pushWeight(float kg) {
  wBuf[wIdx] = kg;
  wIdx = (wIdx + 1) % kBufN;
  if (wCnt < kBufN) wCnt++;
}

static void clearBuf() { wIdx = 0; wCnt = 0; }

static bool connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 2500) { // shortened
    delay(150);
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool ensureWifiConnected() {
  if (WiFi.status() == WL_CONNECTED) return true;
  wifiOK = connectWifi();
  return wifiOK;
}

void setup() {
  Serial.begin(115200);
  delay(100);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(20);

  lcdOK = lcd.begin(Wire, lcdAddr);
  if (lcdOK) {
    lcd.printLine(0, "Calibrating...");
    if (LCD_ROWS > 1) lcd.printLine(1, "RFID Ready...");
  } else {
    Serial.println("LCD FAIL");
  }

  delay(10);
  rfidOK = rfid.begin(Wire);
  if (lcdOK && LCD_ROWS > 1) {
    lcd.printLine(1, rfidOK ? "RFID Ready..." : "RFID Not Found");
  }

  wifiOK = connectWifi();
  if (lcdOK && LCD_ROWS > 2) {
    lcd.printLine(2, wifiOK ? "Internet Ready..." : "Internet Not Ready");
  }

  if (!initScale()) return;

  if (lcdOK) {
    lcd.printLine(0, "");
    if (LCD_ROWS > 1) lcd.printLine(1, "");
    if (LCD_ROWS > 2) lcd.printLine(2, "");
    if (LCD_ROWS > 3) lcd.printLine(3, "Zeroing...");
  }

  scale.tare(32);
  delay(20);

  float zeroKg = scale.getWeightKg(true);
  scaleReady = (fabsf(zeroKg) <= ZERO_THRESHOLD_KG);
  if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, scaleReady ? "Ready             " : "Zero failed       ");

  state = Idle;
  clearBuf();
  zeroStartMs = 0;
  if (!scaleReady) Serial.println("Zeroing did not reach threshold");
}

static void showWeight(float kg) {
  kg = effectiveWeight(kg);
  char l0[21];
  snprintf(l0, sizeof(l0), "Weight %6.2f kg", kg);
  lcd.printLine(0, String(l0));
}

static void doSendData(const String &id, float kg) {
  // Build URL: https://actual.fishcore.ph/uploadWeightIns/1/{IdNumber}/1/{Weight}
  // id = 1 (fixed), scaleId = 1 (fixed), IdNumber = scanned RFID, Weight = stable weight
  const float effectiveKg = effectiveWeight(kg);
  String url = String(UPLOAD_BASE_URL) + "/" + String(UPLOAD_ID) + "/" + id + "/" + String(UPLOAD_SCALE_ID) + "/" + String(effectiveKg, 2);

  Serial.printf("Uploading GET: %s\n", url.c_str());

  if (!ensureWifiConnected()) {
    Serial.println("WiFi not connected; upload skipped");
    if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, "No internet       ");
    delay(200);
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // avoids TLS cert issues; use CA cert if you want verification

  HTTPClient http;
  if (!http.begin(client, url)) {
    Serial.println("HTTP begin failed");
    if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, "HTTP begin failed ");
    delay(200);
    return;
  }

  http.setTimeout(6000);
  int httpCode = http.GET();
  if (httpCode > 0) {
    Serial.printf("HTTP %d\n", httpCode);
    if (httpCode >= 200 && httpCode < 300) {
      if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, "Sent OK           ");
    } else {
      if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, "Send failed       ");
      String payload = http.getString();
      if (payload.length() > 0) {
        Serial.println(payload);
      }
    }
  } else {
    Serial.printf("HTTP GET failed: %s\n", http.errorToString(httpCode).c_str());
    if (lcdOK && LCD_ROWS > 3) lcd.printLine(3, "HTTP GET failed   ");
  }

  http.end();
  delay(200);
}

void loop() {
  if (!lcdOK) { delay(400); return; }

  float kg = scale.getWeightKg(true);
  float absKg = fabsf(kg);

  // Anything at or below MIN_EFFECTIVE_WEIGHT_KG (300 g) behaves as zero
  bool present = absKg > fmaxf(WEIGHT_DETECT_THRESHOLD_KG, MIN_EFFECTIVE_WEIGHT_KG);
  bool isZero  = absKg <= fmaxf(ZERO_THRESHOLD_KG, MIN_EFFECTIVE_WEIGHT_KG);

  // Update stability buffer
  pushWeight(kg);
  float stddev = bufferStdDev();

  switch (state) {
    case Idle:
      showWeight(kg);
      if (present) {
        state = Weighing;
        stableStartMs = 0;
        weighingStartMs = millis();
        clearBuf();
        lcd.printLine(0, "Weighing...");
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 2) lcd.printLine(2, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
      }
      break;

    case Weighing:
      lcd.printLine(0, "Weighing...");
      if (stddev < STABLE_STDDEV_KG) {
        if (stableStartMs == 0) stableStartMs = millis();
        if (millis() - stableStartMs >= STABLE_MIN_MS) {
          stableWeightKg = bufferMean();
          state = AskId;
          showWeight(stableWeightKg);
          if (LCD_ROWS > 2) lcd.printLine(2, "Please Scan The ID");
          if (LCD_ROWS > 1) lcd.printLine(1, "");
          if (LCD_ROWS > 3) lcd.printLine(3, "");
          zeroStartMs = 0;
        }
      } else {
        stableStartMs = 0;
      }

      // Fallback: use buffer mean if not stable before timeout
      if (present && weighingStartMs != 0 &&
          (millis() - weighingStartMs) >= WEIGHING_TIMEOUT_MS) {
        stableWeightKg = bufferMean();
        state = AskId;
        showWeight(stableWeightKg);
        if (LCD_ROWS > 2) lcd.printLine(2, "Please Scan The ID");
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
        zeroStartMs = 0;
      }
      if (!present) {
        state = Idle;
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 2) lcd.printLine(2, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
      }
      break;

    case AskId: {
      // Restart weighing if weight changes a lot while waiting for ID
      if (present && fabsf(kg - stableWeightKg) > WEIGHT_DETECT_THRESHOLD_KG) {
        state = Weighing;
        stableStartMs = 0;
        weighingStartMs = millis();
        clearBuf();
        lcd.printLine(0, "Weighing...");
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 2) lcd.printLine(2, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
        break;
      }

      showWeight(stableWeightKg);
      String id;
      bool scanned = rfidOK && rfid.poll(id) && id.length() > 0;
      if (scanned) {
        if (LCD_ROWS > 1) lcd.printLine(1, "Sending Data Wait....");
        if (LCD_ROWS > 2) lcd.printLine(2, "Remove The weight..");
        doSendData(id, stableWeightKg);
        state = AwaitRemoval;
      } else {
        // If weight returns to zero and stays there, reset
        if (isZero) {
          if (zeroStartMs == 0) zeroStartMs = millis();
          if (millis() - zeroStartMs >= NO_ID_ZERO_TIMEOUT_MS) {
            state = Idle;
            if (LCD_ROWS > 1) lcd.printLine(1, "");
            if (LCD_ROWS > 2) lcd.printLine(2, "");
            if (LCD_ROWS > 3) lcd.printLine(3, "");
          }
        } else {
          zeroStartMs = 0;
        }
      }
      break;
    }

    case Sending:
      // Unused (merged into AskId/AwaitRemoval)
      break;

    case AwaitRemoval:
      // New weighing cycle if weight increases while waiting
      if (present && fabsf(kg - stableWeightKg) > WEIGHT_DETECT_THRESHOLD_KG) {
        state = Weighing;
        stableStartMs = 0;
        weighingStartMs = millis();
        clearBuf();
        lcd.printLine(0, "Weighing...");
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 2) lcd.printLine(2, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
        break;
      }

      showWeight(stableWeightKg);
      if (isZero) {
        state = Idle;
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 2) lcd.printLine(2, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
        clearBuf();
      }
      break;
  }

  // Serial control: 't' to tare
  if (Serial.available()) {
    char cmd = (char)Serial.read();
    if (cmd == 't' || cmd == 'T') {
      if (LCD_ROWS > 3) lcd.printLine(3, "Tare...");
      scale.tare(32);
      float zeroKg2 = scale.getWeightKg(true);
      scaleReady = (fabsf(zeroKg2) <= ZERO_THRESHOLD_KG);
      if (LCD_ROWS > 3) lcd.printLine(3, scaleReady ? "Tare done" : "Tare not zero");
      state = Idle;
      if (LCD_ROWS > 1) lcd.printLine(1, "");
      if (LCD_ROWS > 2) lcd.printLine(2, "");
      clearBuf();
    }
  }
  // Shorter loop delay for interaction. Overall stability
  // is still governed by STABLE_MIN_MS and the buffer.
  delay(100);
}
