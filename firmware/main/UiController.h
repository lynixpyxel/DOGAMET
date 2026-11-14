#pragma once

#include <stdint.h>
#include "AngleSensor.h"
#include "Config.h"
#include "Measurement.h"

enum class AppState : uint8_t {
  BOOT = 0,
  SENSOR_ERROR,
  READY_LIVE,
  HOLD
};

enum class FeedbackKind : uint8_t {
  NONE = 0,
  SD_BOOT_WARN,
  NEED_HOLD,
  SAVED_OK,
  SD_FAIL,
  NEGATIVE_LD
};

class UiController {
 public:
  UiController();

  bool begin();
  bool isLcdAvailable() const { return lcdAvailable_; }

  void showBootScreen();
  void showFeedback(FeedbackKind kind, uint32_t nowMs, uint32_t param = 0);
  void clearFeedback();

  void render(AppState state, SensorStatus sensorStatus,
              const Measurement& liveMeas, const HoldRecord& holdRec,
              bool sdReady, uint32_t nowMs, bool force = false);

  const char* lastLine1() const { return prevLine1_; }
  const char* lastLine2() const { return prevLine2_; }

  static void formatFixed6x1(float value, char out[7]);

 private:
  void writeIfChanged(const char line1[17], const char line2[17]);
  void buildLines(AppState state, SensorStatus sensorStatus,
                  const Measurement& liveMeas, const HoldRecord& holdRec,
                  bool sdReady, uint32_t nowMs, char line1[17], char line2[17]);

  bool lcdAvailable_;
  char prevLine1_[17];
  char prevLine2_[17];
  uint32_t lastRenderMs_;
  FeedbackKind feedbackKind_;
  uint32_t feedbackUntilMs_;
  uint32_t feedbackParam_;
};

