#pragma once

#include <stdint.h>
#include "Config.h"
#include "Measurement.h"

class Storage {
 public:
  explicit Storage(uint8_t csPin = Config::PIN_SD_CS);

  bool begin();
  bool isReady() const { return initialized_; }
  uint32_t nextId() const { return nextId_; }

  bool appendRecord(const HoldRecord& rec, float calibrationFactor,
                    uint32_t elapsedMs, uint32_t& outSavedId);

 private:
  uint32_t scanNextRecordId();

  uint8_t csPin_;
  bool initialized_;
  uint32_t nextId_;
};

