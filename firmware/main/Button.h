#pragma once

#include <stdint.h>
#include "Config.h"

class Button {
 public:
  explicit Button(uint8_t pin, uint16_t debounceMs = Config::BUTTON_DEBOUNCE_MS);

  void begin();
  bool update(uint32_t nowMs);
  bool isPressed() const { return stableState_ == 0; }

 private:
  uint8_t pin_;
  uint16_t debounceMs_;
  uint8_t stableState_;
  uint8_t lastRawState_;
  uint32_t lastChangeMs_;
};

