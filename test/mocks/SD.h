#pragma once

#include <stdint.h>
#include <stdio.h>
#include <map>
#include <string>
#include "Arduino.h"

#define FILE_READ 0
#define FILE_WRITE 1

namespace MockSdState {
inline bool cardPresent = true;
inline bool forceOpenFail = false;
inline bool forceWriteFail = false;
inline std::map<std::string, std::string> files;

inline void reset(bool clearFiles = true) {
  cardPresent = true;
  forceOpenFail = false;
  forceWriteFail = false;
  if (clearFiles) {
    files.clear();
  }
}
}  // namespace MockSdState

class File {
 public:
  File() = default;
  File(std::string name, int mode)
      : name_(std::move(name)), mode_(mode), valid_(true) {
    if (mode_ == FILE_READ) {
      pos_ = 0;
    } else {
      pending_ = MockSdState::files[name_];
    }
  }

  explicit operator bool() const { return valid_; }

  uint32_t size() const {
    if (!valid_) return 0;
    if (mode_ == FILE_WRITE) {
      return static_cast<uint32_t>(pending_.size());
    }
    auto it = MockSdState::files.find(name_);
    return it == MockSdState::files.end()
               ? 0
               : static_cast<uint32_t>(it->second.size());
  }

  int available() {
    if (!valid_ || mode_ != FILE_READ) return 0;
    const std::string& content = MockSdState::files[name_];
    return pos_ < content.size() ? static_cast<int>(content.size() - pos_) : 0;
  }

  int read() {
    if (!valid_ || mode_ != FILE_READ) return -1;
    const std::string& content = MockSdState::files[name_];
    if (pos_ >= content.size()) return -1;
    return static_cast<unsigned char>(content[pos_++]);
  }

  void clearWriteError() { writeErr_ = 0; }
  int getWriteError() const { return writeErr_; }

  size_t print(const char* s) {
    if (!valid_ || mode_ != FILE_WRITE || MockSdState::forceWriteFail) {
      writeErr_ = 1;
      return 0;
    }
    pending_ += s;
    return strlen(s);
  }
  size_t print(char c) {
    if (!valid_ || mode_ != FILE_WRITE || MockSdState::forceWriteFail) {
      writeErr_ = 1;
      return 0;
    }
    pending_.push_back(c);
    return 1;
  }
  size_t print(int32_t v) {
    char b[24];
    snprintf(b, sizeof(b), "%ld", static_cast<long>(v));
    return print(b);
  }
  size_t print(uint32_t v) {
    char b[24];
    snprintf(b, sizeof(b), "%lu", static_cast<unsigned long>(v));
    return print(b);
  }
  size_t print(float v, int digits = 2) {
    char b[32];
    dtostrf(v, 0, static_cast<unsigned char>(digits), b);
    return print(b);
  }

  void flush() {
    if (valid_ && mode_ == FILE_WRITE && writeErr_ == 0 &&
        !MockSdState::forceWriteFail) {
      MockSdState::files[name_] = pending_;
    } else if (MockSdState::forceWriteFail) {
      writeErr_ = 1;
    }
  }

  void close() {
    if (valid_ && mode_ == FILE_WRITE && writeErr_ == 0 &&
        !MockSdState::forceWriteFail) {
      MockSdState::files[name_] = pending_;
    }
    valid_ = false;
  }

 private:
  std::string name_;
  std::string pending_;
  int mode_ = FILE_READ;
  size_t pos_ = 0;
  bool valid_ = false;
  int writeErr_ = 0;
};

class MockSDClass {
 public:
  bool begin(uint8_t) { return MockSdState::cardPresent; }

  bool exists(const char* path) {
    if (!MockSdState::cardPresent) return false;
    return MockSdState::files.find(path) != MockSdState::files.end();
  }

  File open(const char* path, int mode = FILE_READ) {
    if (!MockSdState::cardPresent || MockSdState::forceOpenFail) {
      return File();
    }
    if (mode == FILE_READ && !exists(path)) {
      return File();
    }
    return File(path, mode);
  }
};

inline MockSDClass SD;

