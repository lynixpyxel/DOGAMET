#include "Storage.h"

#include <Arduino.h>
#include <SD.h>
#include <SPI.h>

Storage::Storage(uint8_t csPin)
    : csPin_(csPin), initialized_(false), nextId_(1) {}

bool Storage::begin() {
  // D10 must be configured OUTPUT for ATmega328P SPI master operation.
  pinMode(csPin_, OUTPUT);
  digitalWrite(csPin_, HIGH);

  if (!SD.begin(csPin_)) {
    initialized_ = false;
    return false;
  }

  initialized_ = true;
  nextId_ = scanNextRecordId();
  return true;
}

uint32_t Storage::scanNextRecordId() {
  if (!SD.exists(Config::CSV_FILENAME)) {
    return 1;
  }

  File file = SD.open(Config::CSV_FILENAME, FILE_READ);
  if (!file) {
    return 1;
  }

  uint32_t maxId = 0;
  uint32_t currentId = 0;
  bool atLineStart = true;
  bool inFirstCol = false;
  bool sawDigit = false;

  while (file.available() > 0) {
    int byteVal = file.read();
    if (byteVal < 0) {
      break;
    }
    char c = static_cast<char>(byteVal);

    if (c == '\r' || c == '\n') {
      if (sawDigit && currentId > maxId) {
        maxId = currentId;
      }
      atLineStart = true;
      inFirstCol = false;
      currentId = 0;
      sawDigit = false;
      continue;
    }

    if (atLineStart) {
      atLineStart = false;
      if (c >= '0' && c <= '9') {
        inFirstCol = true;
        sawDigit = true;
        currentId = static_cast<uint32_t>(c - '0');
      } else {
        inFirstCol = false;
      }
    } else if (inFirstCol) {
      if (c >= '0' && c <= '9') {
        currentId = (currentId * 10UL) + static_cast<uint32_t>(c - '0');
      } else if (c == ',') {
        inFirstCol = false;
      } else {
        inFirstCol = false;
        sawDigit = false;
      }
    }
  }

  if (sawDigit && currentId > maxId) {
    maxId = currentId;
  }

  file.close();
  return maxId + 1UL;
}

bool Storage::appendRecord(const HoldRecord& rec, float calibrationFactor,
                           uint32_t elapsedMs, uint32_t& outSavedId) {
  if (!rec.valid || rec.ldCm < 0.0f) {
    return false;
  }

  if (!initialized_ && !begin()) {
    return false;
  }

  File file = SD.open(Config::CSV_FILENAME, FILE_WRITE);
  if (!file) {
    initialized_ = false;
    return false;
  }

  uint32_t sizeBefore = file.size();
  file.clearWriteError();

  if (sizeBefore == 0) {
    file.print(
        F("id,ld_cm,bb_kg,model_bb,pb_cm,signed_count,roller_diameter_mm,"
          "calibration_factor,elapsed_ms,source\n"));
  }

  uint32_t assignedId = nextId_;
  file.print(assignedId);
  file.print(',');
  file.print(rec.ldCm, 2);
  file.print(',');
  file.print(rec.bbKg, 2);
  file.print(',');
  file.print(Config::CSV_MODEL_NAME);
  file.print(',');
  if (rec.hasPb) {
    file.print(rec.pbCm, 2);
  }
  file.print(',');
  file.print(rec.signedCount);
  file.print(',');
  file.print(Config::ROLLER_DIAMETER_MM, 1);
  file.print(',');
  file.print(calibrationFactor, 4);
  file.print(',');
  file.print(elapsedMs);
  file.print(',');
  file.print(Config::CSV_SOURCE_NAME);
  file.print('\n');

  bool writeError = (file.getWriteError() != 0);
  file.flush();
  uint32_t sizeAfter = file.size();
  file.close();

  if (writeError || sizeAfter <= sizeBefore) {
    initialized_ = false;
    return false;
  }

  outSavedId = assignedId;
  ++nextId_;
  return true;
}

