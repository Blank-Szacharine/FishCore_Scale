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

### Calibration via Serial
- After flashing and opening the Serial Monitor (115200), follow on‑screen prompts:
	- Remove all weight for taring.
	- Place a known weight, then type its value in grams and press Enter.
- Calibration values are saved automatically. Commands: 't' to re‑tare, 'c' to recalibrate.

## Files
- include/config.h
- src/modules/lcd_display.{h,cpp}
- src/main.cpp
