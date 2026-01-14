#pragma once

// ---------------- I2C (LCD) ----------------
#define I2C_SDA_PIN 21
#define I2C_SCL_PIN 22

// ---------------- I2C (NAU7802 scale) ----------------
// Per provided mapping: SDA -> GPIO16, SCL -> GPIO17, DRDY -> GPIO27
#define SCALE_SDA_PIN 16
#define SCALE_SCL_PIN 17
#define SCALE_DRDY_PIN 27

// ---------------- Calibration ----------------
// Default calibration factor (grams per ADC count).
// Set this to match your load cell and amplifier scaling.
// Example: if 1000 counts = 500 g, factor = 0.5.
#define SCALE_CAL_FACTOR_DEFAULT 1.0f

// ---------------- LCD parameters ----------------
#define LCD_COLS 20
#define LCD_ROWS 4

#define LCD_ADDR_PRIMARY 0x27
#define LCD_ADDR_SECONDARY 0x3F
 
