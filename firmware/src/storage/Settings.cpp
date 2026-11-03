/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "Settings.h"

#include <Preferences.h>

namespace kf {

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Settings::load()
{
    Preferences p;
    p.begin("kf", true);
    uint8_t r = p.getUChar("range", (uint8_t)PlotRange::H6);
    range = (r <= (uint8_t)PlotRange::D7) ? (PlotRange)r : PlotRange::H6;
    uint8_t m = p.getUChar("mode", (uint8_t)SamplingMode::Eco);
    mode = (m <= (uint8_t)SamplingMode::Fast) ? (SamplingMode)m : SamplingMode::Eco;
    auto_sync = p.getBool("autosync", true);
    temp_offset = p.getFloat("toffset", 0.0f);
    last_sync_utc = p.getLong64("lastsync", 0);
    cal.last_cal_utc = p.getLong64("calLast", 0);
    cal.snooze_until_utc = p.getLong64("calSnooze", 0);
    // calBase is new: without it, calLast may be the faked first-boot time (see CalReminder).
    if (p.isKey("calBase")) {
        cal.base_utc = p.getLong64("calBase", 0);
    } else {
        cal.migrateLegacy();
        cal_migrated = true;
    }
    run_no = p.getInt("runNo", 0);
    p.getString("runDir", run_dir, sizeof(run_dir));
    run_start_utc = p.getLong64("runStart", 0);
    if (run_no <= 0 || run_dir[0] == '\0') {
        run_no = 0;
        run_dir[0] = '\0';
    }
    asc_off_persisted = p.getBool("ascOff", false);
    low_batt_off = p.getBool("lowBatt", false);
    sync_running = p.getBool("syncRun", false);
    p.end();
}

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Preferences only writes keys whose value changed, so a full save is cheap on flash.
void Settings::save() const
{
    Preferences p;
    p.begin("kf", false);
    p.putUChar("range", (uint8_t)range);
    p.putUChar("mode", (uint8_t)mode);
    p.putBool("autosync", auto_sync);
    p.putFloat("toffset", temp_offset);
    p.putLong64("lastsync", last_sync_utc);
    p.putLong64("calLast", cal.last_cal_utc);
    p.putLong64("calSnooze", cal.snooze_until_utc);
    p.putLong64("calBase", cal.base_utc);
    p.putInt("runNo", run_no);
    p.putString("runDir", run_dir);
    p.putLong64("runStart", run_start_utc);
    p.putBool("ascOff", asc_off_persisted);
    p.putBool("lowBatt", low_batt_off);
    p.putBool("syncRun", sync_running);
    p.end();
}

} // namespace kf
