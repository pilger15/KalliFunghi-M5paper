/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// User settings and persistent state in NVS (namespace "kf").
#pragma once

#include <stdint.h>
#include "CalReminder.h"
#include "Types.h"

namespace kf {

struct Settings {
    PlotRange range = PlotRange::H6;
    SamplingMode mode = SamplingMode::Eco;
    bool auto_sync = true;
    float temp_offset = 0.0f;
    int64_t last_sync_utc = 0;      // 0 = never synced
    CalReminder cal;                // last_cal_utc 0 = not initialized yet
    bool asc_off_persisted = false; // SCD41 ASC=0 written to its EEPROM once
    bool low_batt_off = false;      // last power-off was for low battery

    void load();
    void save() const;
};

} // namespace kf
