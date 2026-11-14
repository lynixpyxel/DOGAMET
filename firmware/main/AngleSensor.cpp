#include "AngleSensor.h"

#include <Wire.h>
#include "Config.h"

AngleSensor::AngleSensor()
    : status_(SensorStatus::UNINITIALIZED),
      gainWarning_(MagnetGainWarning::NONE),
      lastStatusReg_(0),
      valid_(false),
      hasBaseline_(false),
      rawPrev_(0),
      rawLast_(0) {}

bool AngleSensor::begin() {
  invalidateBaseline();
  int16_t ignoredDelta = 0;
  return poll(ignoredDelta);
}

void AngleSensor::invalidateBaseline() {
  valid_ = false;
  hasBaseline_ = false;
}

int16_t AngleSensor::computeUnwrapDelta(uint16_t rawNow, uint16_t rawPrev) {
  uint16_t cur = rawNow & 0x0FFFU;
  uint16_t prev = rawPrev & 0x0FFFU;
  int16_t delta = static_cast<int16_t>(cur) - static_cast<int16_t>(prev);
  if (delta > Config::AS5600_HALF_REV) {
    delta -= Config::AS5600_COUNTS_PER_REV;
  } else if (delta < -Config::AS5600_HALF_REV) {
    delta += Config::AS5600_COUNTS_PER_REV;
  }
  return delta;
}

const char* AngleSensor::statusText(SensorStatus status) {
  switch (status) {
    case SensorStatus::OK:
      return "OK";
    case SensorStatus::UNINITIALIZED:
      return "UNINIT";
    case SensorStatus::I2C_ERROR:
      return "I2C_ERR";
    case SensorStatus::MAGNET_MISSING:
      return "NO_MAG";
    case SensorStatus::MAGNET_TOO_WEAK:
      return "MAG_WEAK";
    case SensorStatus::MAGNET_TOO_STRONG:
      return "MAG_HIGH";
    default:
      return "ERR";
  }
}

bool AngleSensor::readRegisterBytes(uint8_t reg, uint8_t count, uint8_t* outBuf) {
  Wire.beginTransmission(Config::AS5600_I2C_ADDR);
  Wire.write(reg);
  // Repeated start avoids releasing the bus between register select and read.
  if (Wire.endTransmission(false) != 0) {
    return false;
  }

  uint8_t received = Wire.requestFrom(Config::AS5600_I2C_ADDR, count);
  if (received != count) {
    while (Wire.available() > 0) {
      (void)Wire.read();
    }
    return false;
  }

  for (uint8_t i = 0; i < count; ++i) {
    outBuf[i] = static_cast<uint8_t>(Wire.read());
  }
  return true;
}

SensorStatus AngleSensor::readRawSample(uint16_t& outRaw) {
  gainWarning_ = MagnetGainWarning::NONE;
#if CHECK_AS5600_MAGNET_STATUS
  uint8_t statusReg = 0;
  if (!readRegisterBytes(REG_STATUS, 1, &statusReg)) {
    return SensorStatus::I2C_ERROR;
  }
  lastStatusReg_ = statusReg;
  if ((statusReg & STATUS_MD_MASK) == 0) {
    return SensorStatus::MAGNET_MISSING;
  }
  if ((statusReg & STATUS_ML_MASK) != 0) {
    gainWarning_ = MagnetGainWarning::WEAK;
#if CHECK_AS5600_MAGNET_GAIN
    return SensorStatus::MAGNET_TOO_WEAK;
#endif
  } else if ((statusReg & STATUS_MH_MASK) != 0) {
    gainWarning_ = MagnetGainWarning::STRONG;
#if CHECK_AS5600_MAGNET_GAIN
    return SensorStatus::MAGNET_TOO_STRONG;
#endif
  }
#endif

  uint8_t angleBytes[2] = {0, 0};
  if (!readRegisterBytes(REG_RAW_ANGLE_H, 2, angleBytes)) {
    return SensorStatus::I2C_ERROR;
  }

  outRaw = (static_cast<uint16_t>(angleBytes[0] & 0x0FU) << 8) |
           static_cast<uint16_t>(angleBytes[1]);
  return SensorStatus::OK;
}

bool AngleSensor::readDiagnostics(uint8_t& outStatus, uint8_t& outAgc,
                                  uint16_t& outMagnitude) {
  uint8_t statusReg = 0;
  uint8_t agcReg = 0;
  uint8_t magBytes[2] = {0, 0};
  if (!readRegisterBytes(REG_STATUS, 1, &statusReg) ||
      !readRegisterBytes(REG_AGC, 1, &agcReg) ||
      !readRegisterBytes(REG_MAGNITUDE_H, 2, magBytes)) {
    return false;
  }
  outStatus = statusReg;
  outAgc = agcReg;
  outMagnitude = (static_cast<uint16_t>(magBytes[0] & 0x0FU) << 8) |
                 static_cast<uint16_t>(magBytes[1]);
  return true;
}

bool AngleSensor::poll(int16_t& outDelta) {
  outDelta = 0;
  uint16_t rawNow = 0;
  SensorStatus sampleStatus = readRawSample(rawNow);
  status_ = sampleStatus;

  if (sampleStatus != SensorStatus::OK) {
    invalidateBaseline();
    return false;
  }

  rawLast_ = rawNow;
  valid_ = true;

  // Establish a fresh baseline after boot or recovery without adding a jump delta.
  if (!hasBaseline_) {
    rawPrev_ = rawNow;
    hasBaseline_ = true;
    outDelta = 0;
    return true;
  }

  outDelta = static_cast<int16_t>(computeUnwrapDelta(rawNow, rawPrev_) *
                                  Config::ANGLE_DIRECTION_SIGN);
  rawPrev_ = rawNow;
  return true;
}

