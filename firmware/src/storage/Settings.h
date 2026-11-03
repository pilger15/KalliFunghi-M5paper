/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// User settings and persistent state in NVS (namespace "kf").
#pragma once

#include <stdint.h>
#include "CalReminder.h"
#include "RunLog.h"
#include "Types.h"

namespace kf {

struct Settings {
    PlotRange range = PlotRange::H6;
    SamplingMode mode = SamplingMode::Eco;
    bool auto_sync = true;
    float temp_offset = 0.0f;
    int64_t last_sync_utc = 0;      // 0 = never synced
    CalReminder cal;                // last_cal_utc 0 = never calibrated, base_utc 0 = not set yet
    bool cal_migrated = false;      // not stored: load() converted the old calibration state
    int run_no = 0;                 // current measurement run, 0 = none yet
    char run_dir[kRunDirMax] = "";  // its folder in /log, e.g. m004_2026-11-03
    int64_t run_start_utc = 0;
    bool asc_off_persisted = false; // SCD41 ASC=0 written to its EEPROM once
    bool low_batt_off = false;      // last power-off was for low battery
    bool sync_running = false;      // set while Wi-Fi is up; still set at boot = reset during sync
    // User-selected network (Wi-Fi page), tried before the default hotspot. Empty SSID
    // = none. Plain text in NVS (no flash encryption). Written only by saveWifi /
    // forgetWifi, never by save().
    char wifi_ssid[33] = "";
    char wifi_pass[65] = "";

    /* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
    // Also migrates the calibration state of older firmware (sets cal_migrated; the
    // caller saves).
    void load();
    void save() const;
    void saveWifi(const char* ssid, const char* password);
    void forgetWifi();
};

} // namespace kf
