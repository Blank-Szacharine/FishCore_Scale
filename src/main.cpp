#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "modules/lcd_display.h"
#include "modules/ads1232.h"

LCDDisplay lcd;
ADS1232 ads(ADS1232_DOUT_PIN, ADS1232_SCLK_PIN, ADS1232_A0_PIN, ADS1232_PDWN_PIN);

bool lcdOK = false;
bool adsOK = false;
uint8_t lcdAddr = 0x00;

void setup() {
  Serial.begin(115200);
  delay(150);

  // LCD init + splash
  lcdOK = lcd.begin(Wire, lcdAddr);
  if (lcdOK) {
    lcd.printLine(0, "Weighing Scale Init...");
    char buf[21];
    snprintf(buf, sizeof(buf), "LCD:0x%02X", lcdAddr);
    lcd.printLine(1, String(buf));
  } else {
    Serial.println("LCD FAIL");
  }

  // ADS init
  ads.begin();
  ads.setChannel(1); // channel 1 by default (AINP1/AINN1)

  adsOK = ads.isAvailable(ADS_READY_TIMEOUT_MS);

  String status;
  status += (adsOK ? "ADS OK" : "ADS FAIL");
  status += " , ";
  status += (lcdOK ? "LCD OK" : "LCD FAIL");

  if (lcdOK) lcd.printLine(2, status);
  Serial.println(status);

  if (adsOK) {
    if (lcdOK) lcd.printLine(3, "Zeroing (tare)...");
    ads.tare(ADS_TARE_SAMPLES);
  } else {
    if (lcdOK) lcd.printLine(3, "Check wiring/pins");
  }

  delay(500);
  if (lcdOK) {
    lcd.printLine(0, "");
    lcd.printLine(1, "");
    lcd.printLine(2, "");
    lcd.printLine(3, "");
  }
}

void loop() {
  if (!adsOK) {
    delay(500);
    return;
  }

  int32_t raw = 0;
  if (ads.readRaw(raw, ADS_READY_TIMEOUT_MS)) {
    int32_t tare = ads.offset();
    int32_t diff = raw - tare;

    float weight = ads.toWeight(raw);
    float displayWeight = weight / WEIGHT_UNIT_DIVISOR;

    if (lcdOK) {
      char line0[21];
      snprintf(line0, sizeof(line0), "Weight:%10.3f %s", displayWeight, WEIGHT_UNIT_LABEL);
      lcd.printLine(0, String(line0));

      char line1[21];
      snprintf(line1, sizeof(line1), "Tare:%11ld", (long)tare);
      lcd.printLine(1, String(line1));

      char line2[21];
      snprintf(line2, sizeof(line2), "Diff:%11ld", (long)diff);
      lcd.printLine(2, String(line2));

      char line3[21];
      snprintf(line3, sizeof(line3), "Raw:%12ld", (long)raw);
      lcd.printLine(3, String(line3));
    }

    Serial.printf("raw=%ld diff=%ld tare=%ld weight=%.6f (%s)\n",
                  (long)raw, (long)diff, (long)tare, displayWeight, WEIGHT_UNIT_LABEL);
  } else {
    // Helpful debug if you keep getting timeouts
    Serial.printf("ADS timeout. DOUT=%d\n", digitalRead(ADS1232_DOUT_PIN));
  }

  delay(50);
}
