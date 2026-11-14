#include <Arduino.h>
#include <Wire.h>

#include "AngleSensor.h"
#include "Button.h"
#include "Config.h"
#include "Measurement.h"
#include "SerialInput.h"
#include "Storage.h"
#include "UiController.h"

namespace {

AngleSensor gSensor;
Measurement gMeasurement(Config::CALIBRATION_FACTOR);
Button gBtnNol(Config::PIN_BTN_NOL);
Button gBtnUkur(Config::PIN_BTN_UKUR);
Button gBtnSimpan(Config::PIN_BTN_SIMPAN);
Storage gStorage(Config::PIN_SD_CS);
SerialInput gSerialInput;
UiController gUi;

AppState gState = AppState::BOOT;
HoldRecord gHoldRecord = {false, 0.0f, 0.0f, 0, false, 0.0f};
uint32_t gLastSensorPollMs = 0;
MagnetGainWarning gLastGainWarning = MagnetGainWarning::NONE;

void printSensorDiagnostics() {
  uint8_t stat = 0;
  uint8_t agc = 0;
  uint16_t mag = 0;
  if (gSensor.readDiagnostics(stat, agc, mag)) {
    Serial.print(F(" AS5600(STAT=0x"));
    Serial.print(stat, HEX);
    Serial.print(F(" AGC="));
    Serial.print(agc);
    Serial.print(F(" MAG="));
    Serial.print(mag);
    Serial.print(F(" RAW="));
    Serial.print(gSensor.rawAngle());
    Serial.print(')');
  }
}

void printSerialStatus() {
  Serial.print(F("STATE="));
  switch (gState) {
    case AppState::BOOT:
      Serial.print(F("BOOT"));
      break;
    case AppState::SENSOR_ERROR:
      Serial.print(F("SENSOR_ERROR("));
      Serial.print(AngleSensor::statusText(gSensor.status()));
      Serial.print(')');
      break;
    case AppState::READY_LIVE:
      Serial.print(F("LIVE"));
      break;
    case AppState::HOLD:
      Serial.print(F("HOLD"));
      break;
  }

  float ld = (gState == AppState::HOLD && gHoldRecord.valid)
                 ? gHoldRecord.ldCm
                 : gMeasurement.lengthCm();
  Serial.print(F(" LD="));
  Serial.print(ld, 2);
  Serial.print(F("cm"));

  float bb = 0.0f;
  if (gState == AppState::HOLD && gHoldRecord.valid) {
    Serial.print(F(" BB_EST="));
    Serial.print(gHoldRecord.bbKg, 2);
    Serial.print(F("kg"));
  } else if (gMeasurement.weightEstimateKg(bb)) {
    Serial.print(F(" BB_EST="));
    Serial.print(bb, 2);
    Serial.print(F("kg"));
  } else {
    Serial.print(F(" BB_EST=NA"));
  }

  Serial.print(F(" COUNT="));
  Serial.print((gState == AppState::HOLD && gHoldRecord.valid)
                   ? gHoldRecord.signedCount
                   : gMeasurement.signedCount());

  Serial.print(F(" PB="));
  if (gSerialInput.hasPb()) {
    Serial.print(gSerialInput.pbCm(), 2);
  } else {
    Serial.print(F("NONE"));
  }

  Serial.print(F(" SD="));
  Serial.print(gStorage.isReady() ? F("OK") : F("ERR"));
  printSensorDiagnostics();
  Serial.println();
}

void handleSerial() {
  SerialCommandEvent ev = gSerialInput.update();
  if (ev == SerialCommandEvent::STATUS_REQUESTED) {
    printSerialStatus();
  }
}

void handleSensor(uint32_t nowMs) {
  if ((nowMs - gLastSensorPollMs) < Config::SENSOR_POLL_INTERVAL_MS) {
    return;
  }
  gLastSensorPollMs = nowMs;

  int16_t delta = 0;
  if (!gSensor.poll(delta)) {
    if (gState != AppState::SENSOR_ERROR) {
      gState = AppState::SENSOR_ERROR;
      gHoldRecord.valid = false;
      gUi.clearFeedback();
      Serial.print(F("[SENSOR] ERR: "));
      Serial.println(AngleSensor::statusText(gSensor.status()));
      gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
                 gStorage.isReady(), nowMs, true);
    }
    return;
  }

  if (gState == AppState::SENSOR_ERROR) {
    gState = AppState::READY_LIVE;
    Serial.println(F("[SENSOR] RECOVERED (fresh baseline)"));
    gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
               gStorage.isReady(), nowMs, true);
  }

  MagnetGainWarning warn = gSensor.gainWarning();
  if (warn != gLastGainWarning) {
    gLastGainWarning = warn;
    if (warn == MagnetGainWarning::WEAK) {
      Serial.print(F("[SENSOR] WARN: ML=1 (magnet weak, reading active)"));
      printSensorDiagnostics();
      Serial.println();
    } else if (warn == MagnetGainWarning::STRONG) {
      Serial.print(F("[SENSOR] WARN: MH=1 (magnet strong, reading active)"));
      printSensorDiagnostics();
      Serial.println();
    }
  }

  gMeasurement.addDelta(delta);
}

void handleButtons(uint32_t nowMs) {
  bool nolPressed = gBtnNol.update(nowMs);
  bool ukurPressed = gBtnUkur.update(nowMs);
  bool simpanPressed = gBtnSimpan.update(nowMs);

  if (nolPressed) {
    if (gState == AppState::SENSOR_ERROR) {
      Serial.println(F("[NOL] IGNORED: sensor invalid"));
    } else {
      gMeasurement.zero();
      gHoldRecord.valid = false;
      gState = AppState::READY_LIVE;
      gUi.clearFeedback();
      Serial.println(F("[NOL] ZEROED -> LIVE"));
      gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
                 gStorage.isReady(), nowMs, true);
    }
  }

  if (ukurPressed) {
    if (gState == AppState::SENSOR_ERROR) {
      Serial.println(F("[UKUR] IGNORED: sensor invalid"));
    } else if (gState == AppState::READY_LIVE) {
      float bbKg = 0.0f;
      if (!gMeasurement.weightEstimateKg(bbKg)) {
        gUi.showFeedback(FeedbackKind::NEGATIVE_LD, nowMs);
        Serial.println(F("[UKUR] REJECTED: negative LD, press NOL"));
      } else {
        gHoldRecord.valid = true;
        gHoldRecord.ldCm = gMeasurement.lengthCm();
        gHoldRecord.bbKg = bbKg;
        gHoldRecord.signedCount = gMeasurement.signedCount();
        gHoldRecord.hasPb = gSerialInput.hasPb();
        gHoldRecord.pbCm = gSerialInput.pbCm();
        gState = AppState::HOLD;
        gUi.clearFeedback();
        Serial.print(F("[UKUR] HOLD LD="));
        Serial.print(gHoldRecord.ldCm, 2);
        Serial.print(F(" BB_EST="));
        Serial.println(gHoldRecord.bbKg, 2);
      }
      gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
                 gStorage.isReady(), nowMs, true);
    } else if (gState == AppState::HOLD) {
      gHoldRecord.valid = false;
      gState = AppState::READY_LIVE;
      gUi.clearFeedback();
      Serial.println(F("[UKUR] RESUME -> LIVE"));
      gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
                 gStorage.isReady(), nowMs, true);
    }
  }

  if (simpanPressed) {
    if (gState != AppState::HOLD || !gHoldRecord.valid) {
      gUi.showFeedback(FeedbackKind::NEED_HOLD, nowMs);
      Serial.println(F("[SIMPAN] IGNORED: press UKUR to HOLD first"));
      gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
                 gStorage.isReady(), nowMs, true);
      return;
    }

    uint32_t savedId = 0;
    if (gStorage.appendRecord(gHoldRecord, gMeasurement.calibrationFactor(),
                              nowMs, savedId)) {
      gUi.showFeedback(FeedbackKind::SAVED_OK, nowMs, savedId);
      Serial.print(F("[SIMPAN] SAVED id="));
      Serial.println(savedId);
    } else {
      gUi.showFeedback(FeedbackKind::SD_FAIL, nowMs);
      Serial.println(F("[SIMPAN] ERR: SD write failed"));
    }
    gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
               gStorage.isReady(), nowMs, true);
  }
}

}  // namespace

void setup() {
  Serial.begin(Config::SERIAL_BAUD_RATE);
  gSerialInput.begin();
  Serial.println(F("DOGAMET booting..."));

  Wire.begin();
  Wire.setClock(Config::I2C_CLOCK_HZ);
#ifdef WIRE_HAS_TIMEOUT
  Wire.setWireTimeout(Config::I2C_TIMEOUT_US, true);
#endif

  gBtnNol.begin();
  gBtnUkur.begin();
  gBtnSimpan.begin();

  if (!gUi.begin()) {
    Serial.print(F("[LCD] WARN: no ACK at 0x"));
    Serial.println(Config::LCD_I2C_ADDR, HEX);
  } else {
    gUi.showBootScreen();
  }

  bool sensorOk = gSensor.begin();
  bool sdOk = gStorage.begin();
  if (!sdOk) {
    Serial.println(F("[SD] WARN: init failed; save disabled"));
  } else {
    Serial.print(F("[SD] OK next_id="));
    Serial.println(gStorage.nextId());
  }

  uint32_t nowMs = millis();
  gLastSensorPollMs = nowMs;

  if (!sensorOk) {
    gState = AppState::SENSOR_ERROR;
    Serial.print(F("[SENSOR] ERR on boot: "));
    Serial.println(AngleSensor::statusText(gSensor.status()));
  } else {
    gState = AppState::READY_LIVE;
    if (!sdOk) {
      gUi.showFeedback(FeedbackKind::SD_BOOT_WARN, nowMs);
    }
  }

  gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
             gStorage.isReady(), nowMs, true);
}

void loop() {
  uint32_t nowMs = millis();
  handleSerial();
  handleSensor(nowMs);
  handleButtons(nowMs);
  gUi.render(gState, gSensor.status(), gMeasurement, gHoldRecord,
             gStorage.isReady(), nowMs, false);
}

