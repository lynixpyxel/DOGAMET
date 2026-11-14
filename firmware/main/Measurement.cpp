#include "Measurement.h"

#include <limits.h>

Measurement::Measurement(float calibrationFactor)
    : signedCount_(0),
      calibrationFactor_(calibrationFactor > 0.0f ? calibrationFactor : 1.0f),
      overflowed_(false) {}

void Measurement::addDelta(int16_t delta) {
  if (overflowed_) {
    return;
  }

  if (delta > 0 && signedCount_ > INT32_MAX - delta) {
    signedCount_ = INT32_MAX;
    overflowed_ = true;
    return;
  }
  if (delta < 0 && signedCount_ < INT32_MIN - delta) {
    signedCount_ = INT32_MIN;
    overflowed_ = true;
    return;
  }

  signedCount_ += static_cast<int32_t>(delta);
}

void Measurement::zero() {
  signedCount_ = 0;
  overflowed_ = false;
}

float Measurement::lengthCm() const {
  return (static_cast<float>(signedCount_) * Config::ROLLER_CIRCUMFERENCE_CM /
          static_cast<float>(Config::AS5600_COUNTS_PER_REV)) *
         calibrationFactor_;
}

bool Measurement::isLengthValidForEstimate() const {
  if (overflowed_) {
    return false;
  }
  return lengthCm() >= 0.0f;
}

float Measurement::computeSchoorlWeightKg(float lengthCm) {
  if (lengthCm < 0.0f) {
    return 0.0f;
  }
  float sum = lengthCm + Config::SCHOORL_OFFSET_CM;
  return (sum * sum) / Config::SCHOORL_DIVISOR;
}

bool Measurement::weightEstimateKg(float& outKg) const {
  if (!isLengthValidForEstimate()) {
    return false;
  }
  outKg = computeSchoorlWeightKg(lengthCm());
  return true;
}

