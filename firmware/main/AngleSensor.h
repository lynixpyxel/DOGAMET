#pragma once

#include <stdint.h>

enum class SensorStatus : uint8_t {
  OK = 0,
  UNINITIALIZED,
  I2C_ERROR,
  MAGNET_MISSING,
  MAGNET_TOO_WEAK,
  MAGNET_TOO_STRONG
};

enum class MagnetGainWarning : uint8_t {
  NONE = 0,
  WEAK,
  STRONG
};

class AngleSensor {
 public:
  AngleSensor();

  bool begin();
  bool poll(int16_t& outDelta);
  void invalidateBaseline();

  bool isValid() const { return valid_; }
  bool hasBaseline() const { return hasBaseline_; }
  SensorStatus status() const { return status_; }
  MagnetGainWarning gainWarning() const { return gainWarning_; }
  uint8_t lastStatusReg() const { return lastStatusReg_; }
  uint16_t rawAngle() const { return rawLast_; }

  bool readDiagnostics(uint8_t& outStatus, uint8_t& outAgc,
                       uint16_t& outMagnitude);

  static int16_t computeUnwrapDelta(uint16_t rawNow, uint16_t rawPrev);
  static const char* statusText(SensorStatus status);

 private:
  static constexpr uint8_t REG_STATUS = 0x0B;
  static constexpr uint8_t REG_RAW_ANGLE_H = 0x0C;
  static constexpr uint8_t REG_AGC = 0x1A;
  static constexpr uint8_t REG_MAGNITUDE_H = 0x1B;
  static constexpr uint8_t STATUS_MD_MASK = 0x20;
  static constexpr uint8_t STATUS_ML_MASK = 0x10;
  static constexpr uint8_t STATUS_MH_MASK = 0x08;

  bool readRegisterBytes(uint8_t reg, uint8_t count, uint8_t* outBuf);
  SensorStatus readRawSample(uint16_t& outRaw);

  SensorStatus status_;
  MagnetGainWarning gainWarning_;
  uint8_t lastStatusReg_;
  bool valid_;
  bool hasBaseline_;
  uint16_t rawPrev_;
  uint16_t rawLast_;
};

