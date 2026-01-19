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
// Calibration factor meaning: ADC counts per gram (not grams per count).
// Start with ~15.0 for many common 5–50kg load cells on NAU7802.
// Increase if your reading is too low; decrease if too high.
#define SCALE_CAL_FACTOR_DEFAULT -13.48f

// ---------------- LCD parameters ----------------
#define LCD_COLS 20
#define LCD_ROWS 4

#define LCD_ADDR_PRIMARY 0x27
#define LCD_ADDR_SECONDARY 0x3F

// ---------------- RFID2 (WS1850S) ----------------
// Shares the same I2C bus as the LCD on GPIO21/22
#define RFID2_ADDR_DEFAULT 0x28
#define RFID2_ADDR_FALLBACK 0x29
#define RFID_RST_PIN 255

// ---------------- WiFi ----------------
#define WIFI_SSID "Bili ka wifi mo 4G"
#define WIFI_PASS "P@ssw0rd549859!"

// ---------------- Weight detection/stability ----------------
#define WEIGHT_DETECT_THRESHOLD_KG 0.05f     // weight present threshold
#define ZERO_THRESHOLD_KG          0.02f     // zero band
#define STABLE_STDDEV_KG           0.007f    // stability stddev threshold
#define STABLE_MIN_MS              3000      // must be stable for this long (ms)
#define NO_ID_ZERO_TIMEOUT_MS      5000      // if no ID scanned, reset after zero held this long

// If the weight never becomes "stable" (stddev below threshold) while present,
// after this many milliseconds we will fall back to the average of the buffer
// and use that as the displayed/recorded weight.
#define WEIGHING_TIMEOUT_MS        3000

// ---------------- Display clamp ----------------
// Prevent -0.00 by clamping very small values to +0.00 before formatting
#define DISPLAY_ZERO_CLAMP_KG      0.005f
