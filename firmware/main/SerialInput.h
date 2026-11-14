#pragma once

#include <stdint.h>

enum class SerialCommandEvent : uint8_t {
  NONE = 0,
  PB_SET,
  PB_CLEARED,
  STATUS_REQUESTED,
  HELP_REQUESTED,
  INVALID_COMMAND
};

class SerialInput {
 public:
  SerialInput();

  void begin();
  SerialCommandEvent update();

  bool hasPb() const { return hasPb_; }
  float pbCm() const { return pbCm_; }

  static bool parsePositiveDecimal(const char* text, float& outValue);

 private:
  static constexpr uint8_t LINE_BUF_SIZE = 32;
  static constexpr uint8_t MAX_BYTES_PER_POLL = 32;

  SerialCommandEvent processLine(char* line);

  bool hasPb_;
  float pbCm_;
  char lineBuf_[LINE_BUF_SIZE];
  uint8_t lineLen_;
  bool overflow_;
};

