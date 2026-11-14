#pragma once

#include <stdint.h>
#include "Config.h"

struct HoldRecord {
  bool valid;
  float ldCm;
  float bbKg;
  int32_t signedCount;
  bool hasPb;
  float pbCm;
};

class Measurement {
 public:
  explicit Measurement(float calibrationFactor = Config::CALIBRATION_FACTOR);

  void addDelta(int16_t delta);
  void zero();

  int32_t signedCount() const { return signedCount_; }
  float calibrationFactor() const { return calibrationFactor_; }
  bool hasOverflow() const { return overflowed_; }

  float lengthCm() const;
  bool isLengthValidForEstimate() const;
  bool weightEstimateKg(float& outKg) const;

  static float computeSchoorlWeightKg(float lengthCm);

 private:
  int32_t signedCount_;
  float calibrationFactor_;
  bool overflowed_;
};

