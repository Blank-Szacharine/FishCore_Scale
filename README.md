# FishCore_Scale

Module-structured ESP32 weighing scale using ADS1232 and 20x4 I2C LCD (PCF8574 backpack).

## Wiring (per user)

LCD (20x4, PCF8574 backpack)
- SCL -> ESP32 D22 (GPIO22)
- SDA -> ESP32 D21 (GPIO21)
- VCC -> VIN (5V)
- GND -> GND

ADS1232 (to ESP32 and load cell)
- DOUT -> ESP32 D34 (GPIO34)
- SCLK -> ESP32 D4 (GPIO4)
- REFP -> to 3.3V and E+ (load cell)
- GND  -> ESP32 GND and E- (load cell)
- AINP -> S+ (load cell)
- AINN -> S- (load cell)
- GAIN0 -> 3.3V (gain strap)
- GAIN1 -> 3.3V (gain strap)
- SPEED -> GND (10 SPS)
- PDWN  -> 3.3V (always on)
- A0    -> GND (Channel 1)

## Boot flow
1. Display: `Weighing Scale Initializing..`
2. Probe LCD and ADS1232; display `ADS OK , LCD OK` or `FAIL` equivalents
3. Tare (auto-zero) the scale
4. Continuously show current weight

## Configuration
See include/config.h
- I2C pins, LCD size
- ADS1232 pins
- `CALIBRATION_FACTOR` for converting raw counts to grams (default 1.0)

To calibrate: place a known weight, note the displayed raw (Serial logs `raw=...`).
Set `CALIBRATION_FACTOR = known_grams / (raw - offset)` and rebuild.

## Build

```bash
pio run
pio run -t upload
pio device monitor -b 115200
```

## Files
- include/config.h
- src/modules/lcd_display.{h,cpp}
- src/modules/ads1232.{h,cpp}
- src/main.cpp
