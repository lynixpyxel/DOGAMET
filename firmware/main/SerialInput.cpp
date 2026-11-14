#include "SerialInput.h"

#include <Arduino.h>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

namespace {

char* trimWhitespace(char* str) {
  while (*str != '\0' && isspace(static_cast<unsigned char>(*str))) {
    ++str;
  }
  size_t len = strlen(str);
  while (len > 0 && isspace(static_cast<unsigned char>(str[len - 1]))) {
    str[len - 1] = '\0';
    --len;
  }
  return str;
}

bool equalsIgnoreCase(const char* a, const char* b) {
  while (*a != '\0' && *b != '\0') {
    if (toupper(static_cast<unsigned char>(*a)) !=
        toupper(static_cast<unsigned char>(*b))) {
      return false;
    }
    ++a;
    ++b;
  }
  return *a == '\0' && *b == '\0';
}

bool startsWithIgnoreCase(const char* str, const char* prefix) {
  while (*prefix != '\0') {
    if (*str == '\0') {
      return false;
    }
    if (toupper(static_cast<unsigned char>(*str)) !=
        toupper(static_cast<unsigned char>(*prefix))) {
      return false;
    }
    ++str;
    ++prefix;
  }
  return true;
}

}  // namespace

SerialInput::SerialInput()
    : hasPb_(false), pbCm_(0.0f), lineLen_(0), overflow_(false) {
  lineBuf_[0] = '\0';
}

void SerialInput::begin() {
  lineLen_ = 0;
  overflow_ = false;
  lineBuf_[0] = '\0';
}

bool SerialInput::parsePositiveDecimal(const char* text, float& outValue) {
  if (text == nullptr || *text == '\0') {
    return false;
  }

  bool sawDigit = false;
  bool sawDot = false;
  for (const char* p = text; *p != '\0'; ++p) {
    if (*p >= '0' && *p <= '9') {
      sawDigit = true;
    } else if (*p == '.' && !sawDot) {
      sawDot = true;
    } else {
      return false;
    }
  }

  if (!sawDigit) {
    return false;
  }

  float val = static_cast<float>(atof(text));
  if (!isfinite(val) || val <= 0.0f) {
    return false;
  }

  outValue = val;
  return true;
}

SerialCommandEvent SerialInput::processLine(char* rawLine) {
  char* line = trimWhitespace(rawLine);
  if (*line == '\0') {
    return SerialCommandEvent::NONE;
  }

  if (startsWithIgnoreCase(line, "PB=")) {
    char* arg = trimWhitespace(line + 3);
    if (equalsIgnoreCase(arg, "CLEAR")) {
      hasPb_ = false;
      pbCm_ = 0.0f;
      Serial.println(F("OK: PB CLEARED"));
      return SerialCommandEvent::PB_CLEARED;
    }

    float parsed = 0.0f;
    if (!parsePositiveDecimal(arg, parsed)) {
      if (strchr(arg, ',') != nullptr) {
        Serial.println(F("ERR: USE DOT DECIMAL (PB=78.5)"));
      } else {
        Serial.println(F("ERR: INVALID PB (USE PB=<POSITIVE_CM> OR PB=CLEAR)"));
      }
      return SerialCommandEvent::INVALID_COMMAND;
    }

    pbCm_ = parsed;
    hasPb_ = true;
    Serial.print(F("OK: PB="));
    Serial.print(pbCm_, 2);
    Serial.println(F(" cm (metadata only, not in Schoorl)"));
    return SerialCommandEvent::PB_SET;
  }

  if (equalsIgnoreCase(line, "STATUS")) {
    return SerialCommandEvent::STATUS_REQUESTED;
  }

  if (equalsIgnoreCase(line, "HELP")) {
    Serial.println(F("CMDS: PB=<cm> | PB=CLEAR | STATUS | HELP"));
    return SerialCommandEvent::HELP_REQUESTED;
  }

  Serial.println(F("ERR: UNKNOWN CMD (HELP)"));
  return SerialCommandEvent::INVALID_COMMAND;
}

SerialCommandEvent SerialInput::update() {
  uint8_t processed = 0;
  while (Serial.available() > 0 && processed < MAX_BYTES_PER_POLL) {
    int ch = Serial.read();
    if (ch < 0) {
      break;
    }
    ++processed;

    char c = static_cast<char>(ch);
    if (c == '\r' || c == '\n') {
      if (overflow_) {
        overflow_ = false;
        lineLen_ = 0;
        lineBuf_[0] = '\0';
        Serial.println(F("ERR: LINE TOO LONG"));
        return SerialCommandEvent::INVALID_COMMAND;
      }
      if (lineLen_ > 0) {
        lineBuf_[lineLen_] = '\0';
        lineLen_ = 0;
        return processLine(lineBuf_);
      }
      continue;
    }

    if (overflow_) {
      continue;
    }

    if (lineLen_ < (LINE_BUF_SIZE - 1)) {
      lineBuf_[lineLen_++] = c;
    } else {
      overflow_ = true;
    }
  }
  return SerialCommandEvent::NONE;
}

