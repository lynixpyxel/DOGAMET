#pragma once

#include <stdint.h>

namespace Config {

// Hardware pin mapping for Arduino Nano (ATmega328P, 5 V / 16 MHz).
// Hardware I2C uses A4 (SDA) and A5 (SCL).
// Hardware SPI uses D11 (MOSI), D12 (MISO), D13 (SCK).
constexpr uint8_t PIN_BTN_NOL = 2;
constexpr uint8_t PIN_BTN_UKUR = 3;
constexpr uint8_t PIN_BTN_SIMPAN = 4;
constexpr uint8_t PIN_SD_CS = 10;  // Must remain OUTPUT for SPI master mode.

// I2C addresses and bus parameters.
constexpr uint8_t AS5600_I2C_ADDR = 0x36;
constexpr uint8_t LCD_I2C_ADDR = 0x27;  // Verify with i2c_scanner; common alternatives: 0x3F.
constexpr uint8_t LCD_COLS = 16;
constexpr uint8_t LCD_ROWS = 2;
constexpr uint32_t I2C_CLOCK_HZ = 100000UL;
constexpr uint32_t I2C_TIMEOUT_US = 3000UL;

// Roller diameter overrides the 20.0 mm value in the original HTML concept.
constexpr float ROLLER_DIAMETER_MM = 24.0f;
constexpr float PI_VALUE = 3.14159265f;
constexpr float ROLLER_CIRCUMFERENCE_CM = (PI_VALUE * ROLLER_DIAMETER_MM) / 10.0f;
constexpr int16_t AS5600_COUNTS_PER_REV = 4096;
constexpr int16_t AS5600_HALF_REV = 2048;
// Set to -1 when forward tape pull decreases AS5600 raw angle (with DIR=GND).
constexpr int8_t ANGLE_DIRECTION_SIGN = -1;
constexpr float CALIBRATION_FACTOR = 1.0f;

// Schoorl weight estimation constants: BB_kg = ((LD_cm + 22)^2) / 100.
constexpr float SCHOORL_OFFSET_CM = 22.0f;
constexpr float SCHOORL_DIVISOR = 100.0f;

// Non-blocking loop intervals and UI timings.
// 5 ms polling (200 Hz) keeps <180 deg/sample valid up to ~7.5 m/s tape speed.
constexpr uint16_t SENSOR_POLL_INTERVAL_MS = 5;
constexpr uint16_t LCD_REFRESH_INTERVAL_MS = 100;
constexpr uint16_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint16_t FEEDBACK_DURATION_MS = 1800;

// Require MD (magnet detected) as a hard validity gate.
// Keep CHECK_AS5600_MAGNET_GAIN at 0 so ML/MH (AGC gain limit) only warns via
// Serial instead of blocking measurement when MD=1.
#define CHECK_AS5600_MAGNET_STATUS 1
#define CHECK_AS5600_MAGNET_GAIN 0

// SD CSV configuration (8.3 FAT filename).
constexpr const char* CSV_FILENAME = "DOGAMET.CSV";
constexpr const char* CSV_MODEL_NAME = "SCHOORL";
constexpr const char* CSV_SOURCE_NAME = "DEVICE";

constexpr uint32_t SERIAL_BAUD_RATE = 115200UL;

}  // namespace Config

