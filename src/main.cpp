// ESP32 + NAU7802 load cell with 20x4 I2C LCD
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <WiFi.h>
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
// New: track WiFi
bool wifiOK = false;

// New: simple stability buffer (smaller for faster lock)
static const int kBufN = 10;
float wBuf[kBufN] = {0};
int wIdx = 0, wCnt = 0;

// New: app states
enum AppState { Idle, Weighing, AskId, Sending, AwaitRemoval };
AppState state = Idle;
unsigned long stableStartMs = 0;
unsigned long zeroStartMs = 0;
float stableWeightKg = 0.0f;

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

void setup() {
  Serial.begin(115200);
  delay(100);

  // Init I2C
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  Wire.setClock(100000);
  delay(20);

  // Initialize LCD
  lcdOK = lcd.begin(Wire, lcdAddr);
  if (lcdOK) {
    lcd.printLine(0, "Calibrating...");
    if (LCD_ROWS > 1) lcd.printLine(1, "RFID Ready...");
  } else {
    Serial.println("LCD FAIL");
  }

  // Initialize RFID2 (status on line 1)
  delay(10);
  rfidOK = rfid.begin(Wire);
  if (lcdOK && LCD_ROWS > 1) {
    lcd.printLine(1, rfidOK ? "RFID Ready..." : "RFID Not Found");
  }

  // Connect to WiFi and show status on line 2
  wifiOK = connectWifi();
  if (lcdOK && LCD_ROWS > 2) {
    lcd.printLine(2, wifiOK ? "Internet Ready..." : "Internet Not Ready");
  }

  // Initialize scale (module tares automatically)
  if (!initScale()) return;

  // Clear status and show Zeroing...
  if (lcdOK) {
    lcd.printLine(0, "");
    if (LCD_ROWS > 1) lcd.printLine(1, "");
    if (LCD_ROWS > 2) lcd.printLine(2, "");
    if (LCD_ROWS > 3) lcd.printLine(3, "Zeroing...");
  }

  // Quick zero before first weight
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

// Clamp small values to avoid printing -0.00
static void showWeight(float kg) {
  if (fabsf(kg) < DISPLAY_ZERO_CLAMP_KG) kg = 0.0f;
  char l0[21];
  snprintf(l0, sizeof(l0), "Weight %6.2f kg", kg);
  lcd.printLine(0, String(l0));
}

static void doSendData(const String &id, float kg) {
  // Placeholder for HTTP/MQTT post
  Serial.printf("Sending: ID=%s, Weight=%.2f kg\n", id.c_str(), kg);
  delay(200);
}

void loop() {
  if (!lcdOK) { delay(400); return; }

  // Always read live weight, but only display numeric in allowed states
  float kg = scale.getWeightKg(true);

  bool present = fabsf(kg) > WEIGHT_DETECT_THRESHOLD_KG;
  bool isZero = fabsf(kg) <= ZERO_THRESHOLD_KG;

  // Update stability buffer
  pushWeight(kg);
  float stddev = bufferStdDev();

  switch (state) {
    case Idle:
      // Show live weight only in Idle (no load or steady zero)
      showWeight(kg);
      if (present) {
        state = Weighing;
        stableStartMs = 0;
        clearBuf();
        lcd.printLine(0, "Weighing...");         // hide numeric while unstable
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 2) lcd.printLine(2, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
      }
      break;

    case Weighing:
      // Keep showing only "Weighing..." while unstable
      lcd.printLine(0, "Weighing...");
      if (stddev < STABLE_STDDEV_KG) {
        if (stableStartMs == 0) stableStartMs = millis();
        if (millis() - stableStartMs >= STABLE_MIN_MS) {
          stableWeightKg = bufferMean();
          state = AskId;
          showWeight(stableWeightKg);           // show numeric only when stable
          if (LCD_ROWS > 2) lcd.printLine(2, "Please Scan The ID");
          if (LCD_ROWS > 1) lcd.printLine(1, "");
          if (LCD_ROWS > 3) lcd.printLine(3, "");
          zeroStartMs = 0;
        }
      } else {
        stableStartMs = 0;
      }
      if (!present) {
        state = Idle;
        if (LCD_ROWS > 1) lcd.printLine(1, "");
        if (LCD_ROWS > 2) lcd.printLine(2, "");
        if (LCD_ROWS > 3) lcd.printLine(3, "");
      }
      break;

    case AskId: {
      // Keep showing the stable weight while asking for ID
      showWeight(stableWeightKg);
      String id;
      bool scanned = rfidOK && rfid.poll(id) && id.length() > 0;
      if (scanned) {
        if (LCD_ROWS > 1) lcd.printLine(1, "Sending Data Wait....");
        if (LCD_ROWS > 2) lcd.printLine(2, "Remove The weight..");
        doSendData(id, stableWeightKg);
        state = AwaitRemoval;
      } else {
        // If weight returns to zero and stays zero for timeout, reset
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
      // unused (merged into AskId/AwaitRemoval)
      break;

    case AwaitRemoval:
      // Keep prompts; return to Idle when zero (then live numeric resumes)
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

  // Optional serial control: 't' to re-tare
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
  // Shorter loop delay for snappier interaction. Overall stability
  // is still governed by STABLE_MIN_MS and the rolling buffer.
  delay(100);
}
