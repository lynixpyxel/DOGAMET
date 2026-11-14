#pragma once

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>

#define HIGH 0x1
#define LOW 0x0
#define INPUT 0x0
#define OUTPUT 0x1
#define INPUT_PULLUP 0x2
#define DEC 10
#define HEX 16
#define F(x) (x)

namespace MockEnv {
inline uint32_t gMillis = 0;
inline uint8_t gPinModes[32] = {0};
inline uint8_t gPinStates[32] = {HIGH};
inline std::string gSerialRx;
inline size_t gSerialRxPos = 0;
inline std::string gSerialTx;

inline void reset() {
  gMillis = 0;
  memset(gPinModes, 0, sizeof(gPinModes));
  for (size_t i = 0; i < 32; ++i) {
    gPinStates[i] = HIGH;
  }
  gSerialRx.clear();
  gSerialRxPos = 0;
  gSerialTx.clear();
}

inline void pushSerialInput(const std::string& s) { gSerialRx += s; }
}  // namespace MockEnv

inline uint32_t millis() { return MockEnv::gMillis; }
inline void delay(uint32_t) {}

inline void pinMode(uint8_t pin, uint8_t mode) {
  if (pin < 32) {
    MockEnv::gPinModes[pin] = mode;
  }
}

inline void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin < 32) {
    MockEnv::gPinStates[pin] = val;
  }
}

inline int digitalRead(uint8_t pin) {
  if (pin < 32) {
    return MockEnv::gPinStates[pin];
  }
  return HIGH;
}

inline char* dtostrf(double val, signed char width, unsigned char prec,
                     char* sout) {
  char fmt[16];
  snprintf(fmt, sizeof(fmt), "%%%d.%df", static_cast<int>(width),
           static_cast<int>(prec));
  snprintf(sout, 32, fmt, val);
  return sout;
}

class MockSerial {
 public:
  void begin(uint32_t) {}

  int available() {
    if (MockEnv::gSerialRxPos >= MockEnv::gSerialRx.size()) {
      return 0;
    }
    return static_cast<int>(MockEnv::gSerialRx.size() - MockEnv::gSerialRxPos);
  }

  int read() {
    if (MockEnv::gSerialRxPos >= MockEnv::gSerialRx.size()) {
      return -1;
    }
    return static_cast<unsigned char>(
        MockEnv::gSerialRx[MockEnv::gSerialRxPos++]);
  }

  size_t print(const char* s) {
    MockEnv::gSerialTx += s;
    return strlen(s);
  }
  size_t print(char c) {
    MockEnv::gSerialTx.push_back(c);
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
  size_t print(uint8_t v, int base = DEC) {
    char b[24];
    snprintf(b, sizeof(b), base == HEX ? "%X" : "%u", static_cast<unsigned>(v));
    return print(b);
  }
  size_t print(uint16_t v, int base = DEC) {
    char b[24];
    snprintf(b, sizeof(b), base == HEX ? "%X" : "%u", static_cast<unsigned>(v));
    return print(b);
  }
  size_t print(float v, int digits = 2) {
    char b[32];
    dtostrf(v, 0, static_cast<unsigned char>(digits), b);
    return print(b);
  }

  size_t println() { return print("\n"); }
  size_t println(const char* s) { return print(s) + println(); }
  size_t println(int32_t v) { return print(v) + println(); }
  size_t println(uint32_t v) { return print(v) + println(); }
  size_t println(uint8_t v, int base = DEC) { return print(v, base) + println(); }
  size_t println(float v, int digits = 2) {
    return print(v, digits) + println();
  }
};

inline MockSerial Serial;
