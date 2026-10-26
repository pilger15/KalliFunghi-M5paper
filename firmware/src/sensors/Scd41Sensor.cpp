/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "Scd41Sensor.h"

#include "config.h"
#include "hw/DebugTrace.h"

namespace kf {

namespace {
constexpr uint8_t kAddr = SCD41_I2C_ADDR_62;
constexpr uint16_t kCmdMeasureSingleShot = 0x219D;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool Scd41Sensor::begin(TwoWire& wire, float temp_offset, bool persist_asc_off)
{
    wire_ = &wire;
    temp_offset_ = temp_offset;
    scd_.begin(wire, kAddr);
    // The sensor may still be measuring from before an ESP reset: stop first (waits 500 ms).
    scd_.stopPeriodicMeasurement();
    uint64_t serial = 0;
    present_ = scd_.getSerialNumber(serial) == 0;
    if (!present_) {
        error_ = true;
        return false;
    }
    if (persist_asc_off) {
        uint16_t asc = 1;
        if (scd_.getAutomaticSelfCalibrationEnabled(asc) == 0 && asc != 0) {
            if (scd_.setAutomaticSelfCalibrationEnabled(0) == 0 && scd_.persistSettings() == 0) {
                asc_persisted_now_ = true;
            }
        } else if (asc == 0) {
            asc_persisted_now_ = true;
        }
    }
    error_ = !applySettings();
    KF_TRACE("scd41 begin ok=%d", (int)!error_);
    return !error_;
}

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Idle state required. All three settings are volatile and re-applied on every
// begin()/reinit(), so a sensor brown-out cannot leave defaults behind.
bool Scd41Sensor::applySettings()
{
    bool ok = scd_.setAutomaticSelfCalibrationEnabled(0) == 0;
    ok = (scd_.setTemperatureOffset(temp_offset_) == 0) && ok;
    ok = (scd_.setSensorAltitude(kSensorAltitudeM) == 0) && ok;
    return ok;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Scd41Sensor::start(uint32_t now)
{
    int16_t err = 0;
    switch (mode_) {
    case SamplingMode::Fast: err = scd_.startPeriodicMeasurement(); break;
    case SamplingMode::Eco: err = scd_.startLowPowerPeriodicMeasurement(); break;
    default: break;   // Ultra: idle, shots on demand
    }
    if (err) { error_ = true; }
    started_ms_ = now;
    discard_next_ = true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Scd41Sensor::setMode(SamplingMode m, uint32_t now)
{
    mode_ = m;
    if (!present_) { return; }
    scd_.stopPeriodicMeasurement();
    start(now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Scd41Sensor::setTemperatureOffset(float offset, uint32_t now)
{
    temp_offset_ = offset;
    if (!present_) { return; }
    scd_.stopPeriodicMeasurement();
    if (scd_.setTemperatureOffset(offset) != 0) { error_ = true; }
    start(now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool Scd41Sensor::consume(Reading& r)
{
    uint16_t co2 = 0;
    float t = 0.0f;
    float rh = 0.0f;
    if (scd_.readMeasurement(co2, t, rh) != 0) {
        error_ = true;
        return false;
    }
    error_ = false;
    if (discard_next_) {
        discard_next_ = false;
        return false;
    }
    // 0 ppm is the sensor's "no valid value" output.
    if (co2 == 0) { return false; }
    r.co2 = co2;
    r.temp_c = t;
    r.rh = rh;
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool Scd41Sensor::poll(Reading& r)
{
    if (!present_ || mode_ == SamplingMode::Ultra) { return false; }
    bool ready = false;
    if (scd_.getDataReadyStatus(ready) != 0) {
        error_ = true;
        return false;
    }
    if (!ready) { return false; }
    return consume(r);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool Scd41Sensor::triggerShot()
{
    if (!present_) { return false; }
    wire_->beginTransmission(kAddr);
    wire_->write((uint8_t)(kCmdMeasureSingleShot >> 8));
    wire_->write((uint8_t)(kCmdMeasureSingleShot & 0xFF));
    bool ok = wire_->endTransmission() == 0;
    if (!ok) { error_ = true; }
    return ok;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool Scd41Sensor::readShot(Reading& r)
{
    if (!present_) { return false; }
    return consume(r);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool Scd41Sensor::forcedRecalibration(uint16_t target_ppm, int& correction_ppm, uint32_t now)
{
    if (!present_) { return false; }
    scd_.stopPeriodicMeasurement();
    uint16_t frc = 0xFFFF;
    int16_t err = scd_.performForcedRecalibration(target_ppm, frc);
    start(now);
    if (err != 0 || frc == 0xFFFF) { return false; }
    correction_ppm = (int)frc - 0x8000;
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool Scd41Sensor::reinit(uint32_t now)
{
    scd_.stopPeriodicMeasurement();
    uint64_t serial = 0;
    present_ = scd_.getSerialNumber(serial) == 0;
    if (!present_) {
        error_ = true;
        return false;
    }
    error_ = !applySettings();
    start(now);
    KF_TRACE("scd41 reinit ok=%d", (int)!error_);
    return !error_;
}

} // namespace kf
