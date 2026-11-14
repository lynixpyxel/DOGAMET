#pragma once

#include <stdint.h>

class MockWire {
 public:
  bool as5600Ack = true;
  bool lcdAck = true;
  bool failNextRequest = false;
  uint8_t statusReg = 0x20;  // MD=1 (magnet detected), ML=0, MH=0.
  uint8_t agcReg = 128;
  uint16_t magnitude = 1500;
  uint16_t rawAngle = 0;

  void begin() {}
  void setClock(uint32_t) {}

  void reset() {
    as5600Ack = true;
    lcdAck = true;
    failNextRequest = false;
    statusReg = 0x20;
    agcReg = 128;
    magnitude = 1500;
    rawAngle = 0;
    txAddr_ = 0;
    regPtr_ = 0;
    rxLen_ = 0;
    rxPos_ = 0;
  }

  void beginTransmission(uint8_t addr) {
    txAddr_ = addr;
    hasReg_ = false;
  }

  size_t write(uint8_t b) {
    if (!hasReg_) {
      regPtr_ = b;
      hasReg_ = true;
    }
    return 1;
  }

  uint8_t endTransmission(bool = true) {
    if (txAddr_ == 0x36) {
      return as5600Ack ? 0 : 2;
    }
    if (txAddr_ == 0x27) {
      return lcdAck ? 0 : 2;
    }
    return 2;
  }

  uint8_t requestFrom(uint8_t addr, uint8_t count) {
    rxLen_ = 0;
    rxPos_ = 0;
    if (addr != 0x36 || !as5600Ack || failNextRequest) {
      failNextRequest = false;
      return 0;
    }
    if (regPtr_ == 0x0B && count == 1) {
      rxBuf_[0] = statusReg;
      rxLen_ = 1;
      return 1;
    }
    if (regPtr_ == 0x0C && count == 2) {
      rxBuf_[0] = static_cast<uint8_t>((rawAngle >> 8) & 0x0F);
      rxBuf_[1] = static_cast<uint8_t>(rawAngle & 0xFF);
      rxLen_ = 2;
      return 2;
    }
    if (regPtr_ == 0x1A && count == 1) {
      rxBuf_[0] = agcReg;
      rxLen_ = 1;
      return 1;
    }
    if (regPtr_ == 0x1B && count == 2) {
      rxBuf_[0] = static_cast<uint8_t>((magnitude >> 8) & 0x0F);
      rxBuf_[1] = static_cast<uint8_t>(magnitude & 0xFF);
      rxLen_ = 2;
      return 2;
    }
    return 0;
  }

  int available() { return static_cast<int>(rxLen_ - rxPos_); }
  int read() {
    if (rxPos_ >= rxLen_) {
      return -1;
    }
    return rxBuf_[rxPos_++];
  }

 private:
  uint8_t txAddr_ = 0;
  uint8_t regPtr_ = 0;
  bool hasReg_ = false;
  uint8_t rxBuf_[4] = {0};
  uint8_t rxLen_ = 0;
  uint8_t rxPos_ = 0;
};

inline MockWire Wire;

