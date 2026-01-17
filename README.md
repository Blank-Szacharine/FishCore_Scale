# FishCore_Scale

ESP32 project with a 20x4 I2C LCD (PCF8574 backpack) and a NAU7802 24‑bit I²C ADC for load cells. On first boot the firmware performs calibration, then continuously displays weight on the LCD.

## Wiring

LCD (20x4, PCF8574 backpack)
- SCL -> ESP32 D22 (GPIO22)
- SDA -> ESP32 D21 (GPIO21)
- VCC -> VIN (5V)
- GND -> GND

NAU7802 (SparkFun Qwiic Scale / NAU7802 breakout)
- VIN -> 3V3
- AV  -> 3V3
- GND -> GND
- SDA -> GPIO16
- SCL -> GPIO17
- DRDY -> GPIO27 (optional)

## Boot Flow
1. Initialize LCD and NAU7802 on separate I²C buses.
2. If no calibration is stored, prompt to remove weight (tare) and then to enter the known weight in grams via the Serial Monitor.
3. Save calibration to NVS (persistent flash).
4. Display live weight and calibration data on the LCD.

## Configuration
See include/config.h
- LCD I²C pins and address candidates (0x27, 0x3F)
- NAU7802 I²C pins (GPIO16/17) and DRDY pin (GPIO27)

## Build

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

### Calibration (No USB required)
- On first boot the device automatically tares (remove all weight) and applies a default calibration factor.
- Set the default factor in include/config.h: `SCALE_CAL_FACTOR_DEFAULT` (grams per count).
- Values are saved automatically. Optional Serial command: 't' to re‑tare when connected.

## RFID2 (WS1850S) Support
- Shares the same I²C bus as the LCD: SDA -> GPIO21, SCL -> GPIO22
- Default address: 0x28 (configurable in include/config.h)
- The UID is shown on LCD row 1 (below the weight).
- Note: readUid implementation is generic. For robust UID reads, replace it with the proper WS1850S command frame or use the official M5Stack UNIT RFID2 library.

## Internet
- Configure WiFi SSID/PASS in include/config.h.
- LCD shows “Internet Ready...” when connected.

## Process Flow
1. Line 0: “Calibrating...”
2. Line 1: “RFID Ready...”
3. Line 2: “Internet Ready...”
4. Clear, then Line 3: “Zeroing...”
5. After ready: show live weight in kg on Line 0.
6. If weight detected: Line 3 “Weighing...” until stable.
7. When stable: keep weight; Line 2 “Please Scan The ID”.
8. After scan: Line 1 “Sending Data please Wait....”; Line 2 “Please Remove The weight..”.
9. When weight returns to zero: clear prompts and return to live weight.
10. If no ID scanned and weight returns to zero for 5 seconds: reset to live weight.

## Display
- Very small values are clamped to 0.00 to avoid “-0.00 kg”.

## Tuning
- Detection/stability thresholds are in include/config.h:
  - WEIGHT_DETECT_THRESHOLD_KG, ZERO_THRESHOLD_KG, STABLE_STDDEV_KG, STABLE_MIN_MS, NO_ID_ZERO_TIMEOUT_MS.

## Files
- include/config.h
- src/modules/lcd_display.{h,cpp}
- src/modules/rfid2.{h,cpp}
- src/modules/scale.cpp
- src/main.cpp
