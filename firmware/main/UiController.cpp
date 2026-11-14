#include "UiController.h"

#include <Arduino.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

LiquidCrystal_I2C gLcd(Config::LCD_I2C_ADDR, Config::LCD_COLS,
                       Config::LCD_ROWS);

void pad16(char out[17], const char* text) {
  size_t len = strlen(text);
  for (uint8_t i = 0; i < Config::LCD_COLS; ++i) {
    out[i] = (i < len) ? text[i] : ' ';
  }
  out[Config::LCD_COLS] = '\0';
}

const char* sensorErrorDetail(SensorStatus status) {
  switch (status) {
    case SensorStatus::I2C_ERROR:
      return "CEK KABEL I2C";
    case SensorStatus::MAGNET_MISSING:
      return "MAGNET HILANG";
    case SensorStatus::MAGNET_TOO_WEAK:
      return "MAGNET LEMAH";
    case SensorStatus::MAGNET_TOO_STRONG:
      return "MAGNET KUAT";
    default:
      return "SENSOR INVALID";
  }
}

}  // namespace

UiController::UiController()
    : lcdAvailable_(false),
      lastRenderMs_(0),
      feedbackKind_(FeedbackKind::NONE),
      feedbackUntilMs_(0),
      feedbackParam_(0) {
  prevLine1_[0] = '\0';
  prevLine2_[0] = '\0';
}

bool UiController::begin() {
  Wire.beginTransmission(Config::LCD_I2C_ADDR);
  uint8_t err = Wire.endTransmission();
  if (err != 0) {
    lcdAvailable_ = false;
    return false;
  }

  gLcd.init();
  gLcd.backlight();
  lcdAvailable_ = true;
  prevLine1_[0] = '\0';
  prevLine2_[0] = '\0';
  return true;
}

void UiController::showBootScreen() {
  char l1[17];
  char l2[17];
  pad16(l1, "DOGAMET v1.0");
  pad16(l2, "INIT SYSTEM...");
  writeIfChanged(l1, l2);
}

void UiController::showFeedback(FeedbackKind kind, uint32_t nowMs,
                                uint32_t param) {
  feedbackKind_ = kind;
  feedbackUntilMs_ = nowMs + Config::FEEDBACK_DURATION_MS;
  feedbackParam_ = param;
}

void UiController::clearFeedback() {
  feedbackKind_ = FeedbackKind::NONE;
  feedbackUntilMs_ = 0;
  feedbackParam_ = 0;
}

void UiController::formatFixed6x1(float value, char out[7]) {
  if (value > 9999.9f || value < -999.9f) {
    strcpy(out, "  OVR ");
    return;
  }
  char tmp[16];
  dtostrf(value, 6, 1, tmp);
  size_t len = strlen(tmp);
  if (len == 6) {
    memcpy(out, tmp, 7);
  } else if (len < 6) {
    uint8_t pad = static_cast<uint8_t>(6 - len);
    for (uint8_t i = 0; i < pad; ++i) {
      out[i] = ' ';
    }
    memcpy(out + pad, tmp, len + 1);
  } else {
    memcpy(out, tmp + (len - 6), 6);
    out[6] = '\0';
  }
}

void UiController::buildLines(AppState state, SensorStatus sensorStatus,
                              const Measurement& liveMeas,
                              const HoldRecord& holdRec, bool sdReady,
                              uint32_t nowMs, char line1[17], char line2[17]) {
  if (feedbackKind_ != FeedbackKind::NONE) {
    if (static_cast<int32_t>(nowMs - feedbackUntilMs_) < 0) {
      char raw1[17];
      char raw2[17];
      switch (feedbackKind_) {
        case FeedbackKind::SD_BOOT_WARN:
          pad16(line1, "SD ERROR");
          pad16(line2, "UKUR SAJA (LIVE)");
          return;
        case FeedbackKind::NEED_HOLD:
          pad16(line1, "TEKAN UKUR DULU");
          pad16(line2, "HARUS MODE HOLD");
          return;
        case FeedbackKind::SAVED_OK: {
          snprintf(raw1, sizeof(raw1), "TERSIMPAN #%lu",
                   static_cast<unsigned long>(feedbackParam_));
          pad16(line1, raw1);
          char bbBuf[7];
          formatFixed6x1(holdRec.bbKg, bbBuf);
          snprintf(raw2, sizeof(raw2), "BB:%s kg EST", bbBuf);
          pad16(line2, raw2);
          return;
        }
        case FeedbackKind::SD_FAIL:
          pad16(line1, "SD ERROR");
          pad16(line2, "GAGAL SIMPAN");
          return;
        case FeedbackKind::NEGATIVE_LD:
          pad16(line1, "LD NEGATIF (<0)");
          pad16(line2, "TEKAN NOL");
          return;
        default:
          break;
      }
    } else {
      clearFeedback();
    }
  }

  if (state == AppState::BOOT) {
    pad16(line1, "DOGAMET v1.0");
    pad16(line2, "INIT SYSTEM...");
    return;
  }

  if (state == AppState::SENSOR_ERROR) {
    pad16(line1, "AS5600 ERROR");
    pad16(line2, sensorErrorDetail(sensorStatus));
    return;
  }

  bool isHold = (state == AppState::HOLD && holdRec.valid);
  float ldCm = isHold ? holdRec.ldCm : liveMeas.lengthCm();
  float bbKg = 0.0f;
  bool validWeight = false;
  if (isHold) {
    bbKg = holdRec.bbKg;
    validWeight = (holdRec.ldCm >= 0.0f);
  } else {
    validWeight = liveMeas.weightEstimateKg(bbKg);
  }

  char ldBuf[7];
  formatFixed6x1(ldCm, ldBuf);
  const char warnChar = sdReady ? ' ' : '!';
  const char modeChar = isHold ? 'H' : 'L';

  char raw1[17];
  snprintf(raw1, sizeof(raw1), "LD:%s cm %c %c", ldBuf, warnChar, modeChar);
  pad16(line1, raw1);

  if (!validWeight) {
    pad16(line2, "TEKAN NOL (LD<0)");
  } else {
    char bbBuf[7];
    formatFixed6x1(bbKg, bbBuf);
    char raw2[17];
    snprintf(raw2, sizeof(raw2), "BB:%s kg EST", bbBuf);
    pad16(line2, raw2);
  }
}

void UiController::writeIfChanged(const char line1[17], const char line2[17]) {
  bool l1Changed = (strcmp(line1, prevLine1_) != 0);
  bool l2Changed = (strcmp(line2, prevLine2_) != 0);

  if (l1Changed) {
    memcpy(prevLine1_, line1, 17);
    if (lcdAvailable_) {
      gLcd.setCursor(0, 0);
      gLcd.print(prevLine1_);
    }
  }
  if (l2Changed) {
    memcpy(prevLine2_, line2, 17);
    if (lcdAvailable_) {
      gLcd.setCursor(0, 1);
      gLcd.print(prevLine2_);
    }
  }
}

void UiController::render(AppState state, SensorStatus sensorStatus,
                          const Measurement& liveMeas,
                          const HoldRecord& holdRec, bool sdReady,
                          uint32_t nowMs, bool force) {
  if (!force && (nowMs - lastRenderMs_) < Config::LCD_REFRESH_INTERVAL_MS) {
    return;
  }
  lastRenderMs_ = nowMs;

  char line1[17];
  char line2[17];
  buildLines(state, sensorStatus, liveMeas, holdRec, sdReady, nowMs, line1,
             line2);
  writeIfChanged(line1, line2);
}

