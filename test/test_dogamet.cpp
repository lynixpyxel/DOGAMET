#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <string>

#include "Arduino.h"
#include "LiquidCrystal_I2C.h"
#include "SD.h"
#include "Wire.h"

#include "AngleSensor.h"
#include "Button.h"
#include "Config.h"
#include "Measurement.h"
#include "SerialInput.h"
#include "Storage.h"
#include "UiController.h"

namespace {

void resetAllMocks() {
  MockEnv::reset();
  Wire.reset();
  MockSdState::reset(true);
  MockLcdState::reset();
}

void assertNear(float actual, float expected, float eps, const char* label) {
  if (fabsf(actual - expected) > eps) {
    fprintf(stderr, "FAIL %s: actual=%.6f expected=%.6f\n", label, actual,
            expected);
    assert(false);
  }
}

void testAngleUnwrapAndWrapAround() {
  resetAllMocks();

  // T-03: Forward wrap 4090 -> 5 gives +11 counts.
  assert(AngleSensor::computeUnwrapDelta(5, 4090) == 11);

  // T-04: Reverse wrap 5 -> 4090 gives -11 counts.
  assert(AngleSensor::computeUnwrapDelta(4090, 5) == -11);

  // Exact half-turn boundary.
  assert(AngleSensor::computeUnwrapDelta(2048, 0) == 2048);
  assert(AngleSensor::computeUnwrapDelta(2049, 0) == -2047);

  // Sensor polling across wrap + disconnect + reconnect without jump (T-20).
  Wire.rawAngle = 4090;
  AngleSensor sensor;
  assert(sensor.begin());
  assert(sensor.isValid());
  assert(sensor.hasBaseline());

  int16_t delta = 0;
  Wire.rawAngle = 5;
  assert(sensor.poll(delta));
  assert(delta == 11 * Config::ANGLE_DIRECTION_SIGN);

  Wire.rawAngle = 4090;
  assert(sensor.poll(delta));
  assert(delta == -11 * Config::ANGLE_DIRECTION_SIGN);

  // Disconnect AS5600 while physical angle moves far away.
  Wire.as5600Ack = false;
  Wire.rawAngle = 2000;
  assert(!sensor.poll(delta));
  assert(!sensor.isValid());
  assert(sensor.status() == SensorStatus::I2C_ERROR);
  assert(delta == 0);

  // Magnet missing fault (MD=0) blocks measurement.
  Wire.as5600Ack = true;
  Wire.statusReg = 0x00;  // MD=0.
  assert(!sensor.poll(delta));
  assert(sensor.status() == SensorStatus::MAGNET_MISSING);

  // With MD=1 and ML=1/MH=1 (default CHECK_AS5600_MAGNET_GAIN=0), measurement
  // stays active with a non-blocking gain warning instead of locking up.
  Wire.statusReg = 0x20 | 0x10;  // MD=1, ML=1.
  Wire.agcReg = 255;
  Wire.magnitude = 180;
  Wire.rawAngle = 2000;
  assert(sensor.poll(delta));
  assert(sensor.isValid());
  assert(sensor.gainWarning() == MagnetGainWarning::WEAK);
  assert(delta == 0);  // Fresh baseline after prior MAGNET_MISSING fault.

  uint8_t diagStat = 0;
  uint8_t diagAgc = 0;
  uint16_t diagMag = 0;
  assert(sensor.readDiagnostics(diagStat, diagAgc, diagMag));
  assert(diagStat == (0x20 | 0x10));
  assert(diagAgc == 255);
  assert(diagMag == 180);

  Wire.statusReg = 0x20 | 0x08;  // MD=1, MH=1.
  Wire.rawAngle = 2010;
  assert(sensor.poll(delta));
  assert(sensor.isValid());
  assert(sensor.gainWarning() == MagnetGainWarning::STRONG);
  assert(delta == 10 * Config::ANGLE_DIRECTION_SIGN);

  // Recover to nominal magnet gain and continue tracking.
  Wire.statusReg = 0x20;
  Wire.rawAngle = 2025;
  assert(sensor.poll(delta));
  assert(sensor.isValid());
  assert(sensor.gainWarning() == MagnetGainWarning::NONE);
  assert(delta == 15 * Config::ANGLE_DIRECTION_SIGN);
}

void testMeasurementFormulasAndSignedDirection() {
  Measurement m(1.0f);

  // T-05: One complete revolution (4096 counts) -> ~7.53982 cm.
  m.addDelta(4096);
  assertNear(m.lengthCm(), 7.539822f, 0.0001f, "full rev cm");

  // T-06: Forward +1000 then reverse -1000 returns to 0.
  m.zero();
  m.addDelta(1000);
  m.addDelta(-1000);
  assert(m.signedCount() == 0);
  assertNear(m.lengthCm(), 0.0f, 0.0001f, "forward+reverse zero");

  // Negative direction stays signed and invalidates Schoorl weight.
  m.addDelta(-500);
  assert(m.signedCount() == -500);
  assert(m.lengthCm() < 0.0f);
  assert(!m.isLengthValidForEstimate());
  float bb = 999.0f;
  assert(!m.weightEstimateKg(bb));

  // Forward again brings it back positive.
  m.addDelta(500);
  assert(m.signedCount() == 0);
  assert(m.isLengthValidForEstimate());

  // T-17: Schoorl arithmetic test: LD = 90.0 cm -> 125.44 kg.
  assertNear(Measurement::computeSchoorlWeightKg(90.0f), 125.44f, 0.001f,
             "schoorl 90cm");

  // Overflow guard.
  m.zero();
  m.addDelta(100);
  // Force near INT32_MAX via zero/not exposing internal, verify normal large counts.
  for (int i = 0; i < 2000; ++i) {
    m.addDelta(4096);
  }
  assert(!m.hasOverflow());
  assert(m.signedCount() == 2000L * 4096L + 100L);
}

void testButtonDebounceAndSingleEdge() {
  resetAllMocks();
  Button btn(Config::PIN_BTN_NOL, 30);
  btn.begin();
  assert(MockEnv::gPinModes[Config::PIN_BTN_NOL] == INPUT_PULLUP);

  // Short bounce (<30 ms) does not trigger.
  MockEnv::gMillis = 100;
  MockEnv::gPinStates[Config::PIN_BTN_NOL] = LOW;
  assert(!btn.update(MockEnv::gMillis));
  MockEnv::gMillis = 115;
  MockEnv::gPinStates[Config::PIN_BTN_NOL] = HIGH;
  assert(!btn.update(MockEnv::gMillis));

  // Stable press (>=30 ms) fires exactly once.
  MockEnv::gMillis = 200;
  MockEnv::gPinStates[Config::PIN_BTN_NOL] = LOW;
  assert(!btn.update(MockEnv::gMillis));
  MockEnv::gMillis = 230;
  assert(btn.update(MockEnv::gMillis));

  // Holding button does not re-trigger.
  MockEnv::gMillis = 300;
  assert(!btn.update(MockEnv::gMillis));
  MockEnv::gMillis = 1000;
  assert(!btn.update(MockEnv::gMillis));

  // Release then re-press triggers again.
  MockEnv::gPinStates[Config::PIN_BTN_NOL] = HIGH;
  MockEnv::gMillis = 1010;
  assert(!btn.update(MockEnv::gMillis));
  MockEnv::gMillis = 1045;
  assert(!btn.update(MockEnv::gMillis));

  MockEnv::gPinStates[Config::PIN_BTN_NOL] = LOW;
  MockEnv::gMillis = 1100;
  assert(!btn.update(MockEnv::gMillis));
  MockEnv::gMillis = 1130;
  assert(btn.update(MockEnv::gMillis));
}

void testSerialPbCommands() {
  resetAllMocks();
  SerialInput si;
  si.begin();
  assert(!si.hasPb());

  // T-18: Valid PB=78.5.
  MockEnv::pushSerialInput("PB=78.5\n");
  assert(si.update() == SerialCommandEvent::PB_SET);
  assert(si.hasPb());
  assertNear(si.pbCm(), 78.5f, 0.001f, "pb 78.5");

  // Malformed values rejected, preserving or not corrupting state.
  MockEnv::pushSerialInput("PB=-5\n");
  assert(si.update() == SerialCommandEvent::INVALID_COMMAND);
  assert(si.hasPb());

  MockEnv::pushSerialInput("PB=0\n");
  assert(si.update() == SerialCommandEvent::INVALID_COMMAND);

  MockEnv::pushSerialInput("PB=78,5\n");
  assert(si.update() == SerialCommandEvent::INVALID_COMMAND);

  MockEnv::pushSerialInput("PB=78.5abc\n");
  assert(si.update() == SerialCommandEvent::INVALID_COMMAND);

  // T-19: PB=CLEAR clears metadata.
  MockEnv::pushSerialInput("PB=CLEAR\n");
  assert(si.update() == SerialCommandEvent::PB_CLEARED);
  assert(!si.hasPb());

  // Buffer overflow recovery (>32 bytes spans two non-blocking polls).
  MockEnv::pushSerialInput("PB=12345678901234567890123456789012345\n");
  assert(si.update() == SerialCommandEvent::NONE);
  assert(si.update() == SerialCommandEvent::INVALID_COMMAND);
  MockEnv::pushSerialInput("PB=82.0\n");
  assert(si.update() == SerialCommandEvent::PB_SET);
  assertNear(si.pbCm(), 82.0f, 0.001f, "pb after overflow");
}

void testStorageCsvAppendPersistenceAndErrors() {
  resetAllMocks();
  Storage storage(Config::PIN_SD_CS);
  assert(storage.begin());
  assert(MockEnv::gPinModes[Config::PIN_SD_CS] == OUTPUT);
  assert(storage.isReady());
  assert(storage.nextId() == 1);

  // T-12: Write first record without PB -> pb_cm blank.
  HoldRecord r1 = {true, 90.0f, 125.44f, 48892, false, 0.0f};
  uint32_t id1 = 0;
  assert(storage.appendRecord(r1, 1.0f, 1234, id1));
  assert(id1 == 1);

  // Second record with PB=78.5 -> pb_cm populated, BB unchanged.
  HoldRecord r2 = {true, 90.0f, 125.44f, 48892, true, 78.5f};
  uint32_t id2 = 0;
  assert(storage.appendRecord(r2, 1.0f, 5678, id2));
  assert(id2 == 2);

  // T-16: Simulate reboot with existing DOGAMET.CSV on card.
  Storage afterReboot(Config::PIN_SD_CS);
  assert(afterReboot.begin());
  assert(afterReboot.nextId() == 3);

  uint32_t id3 = 0;
  assert(afterReboot.appendRecord(r1, 1.0f, 999, id3));
  assert(id3 == 3);

  const std::string& csv = MockSdState::files[Config::CSV_FILENAME];
  assert(csv.find("id,ld_cm,bb_kg,model_bb,pb_cm,signed_count,"
                  "roller_diameter_mm,calibration_factor,elapsed_ms,source\n") ==
         0);
  assert(csv.find("1,90.00,125.44,SCHOORL,,48892,24.0,1.0000,1234,DEVICE\n") !=
         std::string::npos);
  assert(csv.find("2,90.00,125.44,SCHOORL,78.50,48892,24.0,1.0000,5678,DEVICE\n") !=
         std::string::npos);
  assert(csv.find("3,90.00,125.44,SCHOORL,,48892,24.0,1.0000,999,DEVICE\n") !=
         std::string::npos);

  // T-15: Write failure surfaces error and does not increment ID.
  MockSdState::forceWriteFail = true;
  uint32_t idFail = 999;
  assert(!afterReboot.appendRecord(r1, 1.0f, 1111, idFail));
  assert(!afterReboot.isReady());
  assert(afterReboot.nextId() == 4);
  MockSdState::forceWriteFail = false;

  // T-14: Boot without SD card.
  MockSdState::cardPresent = false;
  Storage noCard(Config::PIN_SD_CS);
  assert(!noCard.begin());
  assert(!noCard.isReady());
  assert(!noCard.appendRecord(r1, 1.0f, 2222, idFail));
}

void testUiControllerRenderingAndLcdMissing() {
  resetAllMocks();
  UiController ui;
  assert(ui.begin());
  assert(ui.isLcdAvailable());

  Measurement m(1.0f);
  // 48892 counts ~ 90.0 cm -> BB 125.4 kg.
  for (int i = 0; i < 48; ++i) {
    m.addDelta(1000);
  }
  m.addDelta(892);
  HoldRecord hold = {false, 0.0f, 0.0f, 0, false, 0.0f};

  ui.render(AppState::READY_LIVE, SensorStatus::OK, m, hold, true, 1000, true);
  assert(strlen(ui.lastLine1()) == 16);
  assert(strlen(ui.lastLine2()) == 16);
  assert(std::string(ui.lastLine1()) == "LD:  90.0 cm   L");
  assert(std::string(ui.lastLine2()) == "BB: 125.4 kg EST");

  // Hold mode renders H and freezes values even if live Measurement moves.
  hold = {true, 90.0f, 125.44f, 48892, false, 0.0f};
  m.addDelta(4096);
  ui.render(AppState::HOLD, SensorStatus::OK, m, hold, false, 1200, true);
  assert(std::string(ui.lastLine1()) == "LD:  90.0 cm ! H");
  assert(std::string(ui.lastLine2()) == "BB: 125.4 kg EST");

  // Sensor error renders AS5600 ERROR.
  ui.render(AppState::SENSOR_ERROR, SensorStatus::MAGNET_MISSING, m, hold, true,
            1300, true);
  assert(std::string(ui.lastLine1()) == "AS5600 ERROR    ");
  assert(std::string(ui.lastLine2()) == "MAGNET HILANG   ");

  // LCD missing on boot does not crash and returns false cleanly.
  resetAllMocks();
  Wire.lcdAck = false;
  UiController noLcdUi;
  assert(!noLcdUi.begin());
  assert(!noLcdUi.isLcdAvailable());
  noLcdUi.render(AppState::READY_LIVE, SensorStatus::OK, m, hold, true, 1500,
                 true);
}

void testEndToEndHoldFreezeSaveFlow() {
  resetAllMocks();
  AngleSensor sensor;
  Measurement meas(1.0f);
  Storage storage(Config::PIN_SD_CS);
  UiController ui;
  SerialInput serialInput;

  Wire.rawAngle = 0;
  assert(sensor.begin());
  assert(storage.begin());
  assert(ui.begin());
  serialInput.begin();

  // Rotate ~90 cm (simulate 12 steps of 4074 counts total? No, max <2048/step).
  for (int i = 0; i < 30; ++i) {
    Wire.rawAngle = static_cast<uint16_t>((Wire.rawAngle + 1630) & 0x0FFFU);
    int16_t delta = 0;
    assert(sensor.poll(delta));
    meas.addDelta(delta);
  }
  float liveBeforeHold = meas.lengthCm();
  assert(liveBeforeHold > 80.0f);

  // T-08 / T-09: Snapshot in HOLD while roller keeps moving.
  float bb = 0.0f;
  assert(meas.weightEstimateKg(bb));
  HoldRecord hold = {true, liveBeforeHold, bb, meas.signedCount(), false, 0.0f};

  Wire.rawAngle = static_cast<uint16_t>((Wire.rawAngle + 1500) & 0x0FFFU);
  int16_t deltaDuringHold = 0;
  assert(sensor.poll(deltaDuringHold));
  meas.addDelta(deltaDuringHold);

  // Snapshot remains frozen while live Measurement tracked background rotation.
  assertNear(hold.ldCm, liveBeforeHold, 0.0001f, "hold frozen");
  assert(meas.lengthCm() > liveBeforeHold + 2.0f);

  // Save snapshot.
  uint32_t savedId = 0;
  assert(storage.appendRecord(hold, meas.calibrationFactor(), 5000, savedId));
  assert(savedId == 1);

  // T-07: NOL resets relative count to 0 and clears HOLD.
  meas.zero();
  hold.valid = false;
  assert(meas.signedCount() == 0);
  assertNear(meas.lengthCm(), 0.0f, 0.0001f, "after NOL");
}

}  // namespace

int main() {
  testAngleUnwrapAndWrapAround();
  testMeasurementFormulasAndSignedDirection();
  testButtonDebounceAndSingleEdge();
  testSerialPbCommands();
  testStorageCsvAppendPersistenceAndErrors();
  testUiControllerRenderingAndLcdMissing();
  testEndToEndHoldFreezeSaveFlow();
  printf("ALL DOGAMET HOST TESTS PASSED (7/7 suites)\n");
  return 0;
}
