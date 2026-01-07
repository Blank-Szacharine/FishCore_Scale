#include <Arduino.h>
#include <Wire.h>
#include "config.h"
#include "modules/lcd_display.h"
#include "modules/ads1232.h"

LCDDisplay lcd;
ADS1232 ads(ADS1232_DOUT_PIN, ADS1232_SCLK_PIN);

bool lcdOK = false;
bool adsOK = false;
uint8_t lcdAddr = 0x00;

void setup() {
  Serial.begin(115200);
  delay(100);

  // Initialize LCD and show splash
  lcdOK = lcd.begin(Wire, lcdAddr);
  if (lcdOK) {
    lcd.printLine(0, "Weighing Scale Initializing..");
  }

  // Initialize ADS1232
  ads.begin();

  // Check device availability
  adsOK = ads.isAvailable(ADS_READY_TIMEOUT_MS);

  String status;
  status += (adsOK ? "ADS OK" : "ADS FAIL");
  status += " , ";
  status += (lcdOK ? "LCD OK" : "LCD FAIL");

  if (lcdOK) {
    lcd.printLine(1, status);
  }
  Serial.println(status);

  // If LCD wasn't up initially, try printing over Serial only

  // If ADS OK, tare to zero
  if (adsOK) {
    if (lcdOK) lcd.printLine(2, "Zeroing scale...");
    ads.tare(ADS_TARE_SAMPLES);
  }

  delay(500);
  if (lcdOK) {
    lcd.printLine(0, "                    ");
    lcd.printLine(1, "                    ");
    lcd.printLine(2, "                    ");
    lcd.printLine(3, "                    ");
  }
}

void loop() {
  if (!adsOK) {
    delay(500);
    return;
  }

  int32_t raw;
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
    // Also log to Serial
    Serial.printf("raw=%ld diff=%ld tare=%ld weight=%.6f (%s)\n", (long)raw, (long)diff, (long)tare, displayWeight, WEIGHT_UNIT_LABEL);
  }
  // At 10SPS (SPEED tied to GND), new data ~ every 100ms
  delay(50);
}