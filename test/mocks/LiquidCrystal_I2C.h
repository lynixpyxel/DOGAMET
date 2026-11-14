#pragma once

#include <stdint.h>
#include <string.h>
#include <string>

namespace MockLcdState {
inline std::string rows[2] = {"                ", "                "};
inline uint8_t col = 0;
inline uint8_t row = 0;
inline bool initialized = false;
inline bool backlightOn = false;

inline void reset() {
  rows[0] = "                ";
  rows[1] = "                ";
  col = 0;
  row = 0;
  initialized = false;
  backlightOn = false;
}
}  // namespace MockLcdState

class LiquidCrystal_I2C {
 public:
  LiquidCrystal_I2C(uint8_t, uint8_t, uint8_t) {}
  void init() { MockLcdState::initialized = true; }
  void backlight() { MockLcdState::backlightOn = true; }
  void setCursor(uint8_t c, uint8_t r) {
    MockLcdState::col = c;
    MockLcdState::row = r < 2 ? r : 1;
  }
  size_t print(const char* s) {
    size_t len = strlen(s);
    for (size_t i = 0; i < len && (MockLcdState::col + i) < 16; ++i) {
      MockLcdState::rows[MockLcdState::row][MockLcdState::col + i] = s[i];
    }
    return len;
  }
};

