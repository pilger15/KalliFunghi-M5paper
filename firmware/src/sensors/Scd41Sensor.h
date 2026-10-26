/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// SCD41 driver wrapper.
//
// Command rules (datasheet, SensirionI2cScd4x.h):
//  - ASC and temperature offset are accepted only in idle: stop first.
//  - stop_periodic_measurement needs 500 ms (the library waits inside).
//  - The first reading after a mode change is discarded.
//  - FRC needs >= 3 min of operation in the mode used afterwards, then stop, then FRC.
//  - Single shot (0x219D) takes 5 s; issued here without the library's blocking wait.
//
// Sensor settings are volatile: ASC off, the temperature offset and the altitude
// (kSensorAltitudeM, pressure compensation) are applied on every begin().
// ASC=0 is persisted to the sensor EEPROM once, so a sensor brown-out cannot
// silently re-enable ASC (EEPROM is rated for 2000 writes).
#pragma once

#include <Wire.h>
#include <SensirionI2cScd4x.h>
#include "Types.h"

namespace kf {

class Scd41Sensor {
public:
    struct Reading {
        uint16_t co2;
        float temp_c;
        float rh;
    };

    // Returns false if the sensor does not answer. persist_asc_off: write ASC=0 to EEPROM.
    bool begin(TwoWire& wire, float temp_offset, bool persist_asc_off);
    bool present() const { return present_; }
    // True after begin() wrote ASC=0 to the EEPROM (caller stores it in NVS).
    bool ascPersistedNow() const { return asc_persisted_now_; }

    // Stop, then start the mode (Ultra: stays idle, shots are triggered).
    void setMode(SamplingMode m, uint32_t now);
    SamplingMode mode() const { return mode_; }
    // Changes the temperature offset (stop, set, restart).
    void setTemperatureOffset(float offset, uint32_t now);

    // Periodic modes: true and fills r when a new, non-discarded reading is ready.
    // Sets error() on I2C failure.
    bool poll(Reading& r);
    // Ultra: trigger a single shot; read it 5 s later with readShot().
    bool triggerShot();
    bool readShot(Reading& r);

    // ms of continuous operation in the current mode (FRC precondition).
    uint32_t runningMs(uint32_t now) const { return now - started_ms_; }
    // Forced recalibration. Returns true and the correction in ppm on success.
    bool forcedRecalibration(uint16_t target_ppm, int& correction_ppm, uint32_t now);
    // Re-applies all settings and restarts the current mode (sensor recovery).
    bool reinit(uint32_t now);

    bool error() const { return error_; }

private:
    bool applySettings();
    void start(uint32_t now);
    bool consume(Reading& r);

    SensirionI2cScd4x scd_;
    TwoWire* wire_ = nullptr;
    float temp_offset_ = 0.0f;
    SamplingMode mode_ = SamplingMode::Eco;
    bool present_ = false;
    bool error_ = false;
    bool discard_next_ = true;
    bool asc_persisted_now_ = false;
    uint32_t started_ms_ = 0;
};

} // namespace kf
