#include <Arduino.h>
#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin();
  Wire.setClock(100000UL);
#ifdef WIRE_HAS_TIMEOUT
  Wire.setWireTimeout(3000UL, true);
#endif
  Serial.println(F("DOGAMET I2C Scanner"));
}

void loop() {
  uint8_t foundCount = 0;
  Serial.println(F("Scanning 0x03..0x77..."));

  for (uint8_t addr = 0x03; addr <= 0x77; ++addr) {
    Wire.beginTransmission(addr);
    uint8_t err = Wire.endTransmission();
    if (err == 0) {
      Serial.print(F("  Found 0x"));
      if (addr < 0x10) {
        Serial.print('0');
      }
      Serial.print(addr, HEX);
      if (addr == 0x36) {
        Serial.print(F(" (AS5600)"));
      } else if (addr == 0x27 || addr == 0x3F) {
        Serial.print(F(" (LCD I2C candidate)"));
      }
      Serial.println();
      ++foundCount;
    }
  }

  if (foundCount == 0) {
    Serial.println(F("  No I2C devices found."));
  }
  delay(3000);
}

