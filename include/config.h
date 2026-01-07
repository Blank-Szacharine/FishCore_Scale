#pragma once

// ---------------- I2C (LCD) ----------------
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// ---------------- ADS1232 pins ----------------
// Your wiring:
#define ADS1232_DOUT_PIN 34
#define ADS1232_SCLK_PIN 4

// OPTIONAL: If you wired ADS "A0" to an ESP32 GPIO, set it here.
// If you DID NOT wire A0 to ESP32, set this to -1 and STRAP A0 physically to GND (CH1).
#define ADS1232_A0_PIN   -1

// OPTIONAL: If you wired PDWN to an ESP32 GPIO, set it here.
// If you strapped PDWN to 3.3V, set to -1.
#define ADS1232_PDWN_PIN -1

// ---------------- LCD parameters ----------------
#define LCD_COLS 20
#define LCD_ROWS 4

#define LCD_ADDR_PRIMARY 0x27
#define LCD_ADDR_SECONDARY 0x3F

// ---------------- Calibration / Units ----------------
// Start at 1.0 for debugging.
// Later, replace with your actual calibration factor after you calibrate using known weight.
#define CALIBRATION_FACTOR 1.0f

// Label + divisor:
// If factor produces grams, divisor 1000 shows kg.
// Fix label if you want:
#define WEIGHT_UNIT_LABEL "kg"
#define WEIGHT_UNIT_DIVISOR 1000.0f

// ---------------- Sampling / timeouts ----------------
#define ADS_READY_TIMEOUT_MS 500
#define ADS_TARE_SAMPLES 16
