#include "Button.h"

#include <Arduino.h>

Button::Button(uint8_t pin, uint16_t debounceMs)
    : pin_(pin),
      debounceMs_(debounceMs),
      stableState_(HIGH),
      lastRawState_(HIGH),
      lastChangeMs_(0) {}

void Button::begin() {
  pinMode(pin_, INPUT_PULLUP);
  uint8_t initial = static_cast<uint8_t>(digitalRead(pin_));
  stableState_ = initial;
  lastRawState_ = initial;
  lastChangeMs_ = millis();
}

bool Button::update(uint32_t nowMs) {
  uint8_t raw = static_cast<uint8_t>(digitalRead(pin_));
  if (raw != lastRawState_) {
    lastRawState_ = raw;
    lastChangeMs_ = nowMs;
  }

  if ((nowMs - lastChangeMs_) >= debounceMs_ && raw != stableState_) {
    stableState_ = raw;
    if (stableState_ == LOW) {
      return true;
    }
  }
  return false;
}

