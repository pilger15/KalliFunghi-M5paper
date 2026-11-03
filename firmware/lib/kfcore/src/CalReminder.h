/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Weekly SCD41 calibration reminder. Times are UTC seconds as stored in NVS.
//
// last_cal_utc is a real forced recalibration only (0 = never calibrated). base_utc is
// where the reminder interval starts while there is none, set on first boot so there is
// no reminder on day one. Older firmware wrote that boot time into last_cal_utc;
// migrateLegacy() moves it to base_utc.
#pragma once

#include <stdint.h>

namespace kf {

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
struct CalReminder {
    int64_t last_cal_utc = 0;
    int64_t base_utc = 0;
    int64_t snooze_until_utc = 0;

    bool everCalibrated() const { return last_cal_utc != 0; }
    int64_t intervalStart() const { return last_cal_utc > base_utc ? last_cal_utc : base_utc; }
    // Due when the interval has passed and no snooze is active.
    bool due(int64_t now_utc, int64_t interval_s) const
    {
        return (now_utc - intervalStart()) >= interval_s && now_utc >= snooze_until_utc;
    }
    void calibrated(int64_t now_utc) { last_cal_utc = now_utc; snooze_until_utc = 0; }
    void postpone(int64_t now_utc, int64_t snooze_s) { snooze_until_utc = now_utc + snooze_s; }
    // Old single-field state: its last_cal_utc may be the faked first-boot time, and no
    // calibration was ever logged, so it counts as "never" and only keeps the reminder timing.
    void migrateLegacy() { base_utc = last_cal_utc; last_cal_utc = 0; }
    // Shift stored times by a clock correction (first sync after unsynced use).
    void rebase(int64_t delta_s)
    {
        if (last_cal_utc) { last_cal_utc += delta_s; }
        if (base_utc) { base_utc += delta_s; }
        if (snooze_until_utc) { snooze_until_utc += delta_s; }
    }
};

} // namespace kf
