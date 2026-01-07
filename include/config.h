#pragma once

// Pin configuration based on user wiring
// ESP32 default I2C: SDA = GPIO21 (D21), SCL = GPIO22 (D22)
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// ADS1232 wiring
#define ADS1232_DOUT_PIN 34  // D34 (input only)
#define ADS1232_SCLK_PIN 4   // D4

// LCD parameters
#define LCD_COLS 20
#define LCD_ROWS 4

// Optional: Known common I2C addresses for PCF8574 LCD backpacks
// We'll scan for these; first match wins.
// 0x27 and 0x3F are the most common.
#define LCD_ADDR_PRIMARY 0x27
#define LCD_ADDR_SECONDARY 0x3F

// Weight calibration
// The ADS1232 outputs raw counts. To convert to grams, set this factor
// after calibration (place a known weight and compute factor).
// Default to 1.0 so raw counts are displayed as-is.
#define CALIBRATION_FACTOR 1.0f

// Display unit configuration
// If your CALIBRATION_FACTOR yields grams, set divisor to 1000.0f to display in kilograms.
// The user requested to show unit as "kl". Adjust as needed.
#define WEIGHT_UNIT_LABEL "kl"
#define WEIGHT_UNIT_DIVISOR 1000.0f

// Sampling / timeouts
#define ADS_READY_TIMEOUT_MS 500
#define ADS_TARE_SAMPLES 16
