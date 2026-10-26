/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Weekly SCD41 calibration reminder. Times are UTC seconds as stored in NVS.
#pragma once

#include <stdint.h>

namespace kf {

struct CalReminder {
    int64_t last_cal_utc = 0;
    int64_t snooze_until_utc = 0;

    // Due when the interval has passed and no snooze is active.
    bool due(int64_t now_utc, int64_t interval_s) const
    {
        return (now_utc - last_cal_utc) >= interval_s && now_utc >= snooze_until_utc;
    }
    void calibrated(int64_t now_utc) { last_cal_utc = now_utc; snooze_until_utc = 0; }
    void postpone(int64_t now_utc, int64_t snooze_s) { snooze_until_utc = now_utc + snooze_s; }
    // Shift stored times by a clock correction (first sync after unsynced use).
    void rebase(int64_t delta_s)
    {
        last_cal_utc += delta_s;
        if (snooze_until_utc) { snooze_until_utc += delta_s; }
    }
};

} // namespace kf
