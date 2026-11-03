/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Measurement runs: one folder per grow batch on the SD card.
//
//   /log/error.log, /log/sync.csv      global
//   /log/m004_2026-11-03/              run 4, started 3 Nov 2026 (local date)
//       run.txt                        key=value lines, see formatRunInfo
//       2026-11-03.csv                 one CSV per UTC day (CsvFormat)
//       unsynced.csv                   rows before the first clock sync
//
// A run started before the clock was ever synced is named m001_unsynced. Folders are
// never renamed. The current run (number, folder name, start) lives in NVS.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "TimeUtil.h"

namespace kf {

constexpr const char* kLogDir = "/log";
constexpr const char* kRunInfoName = "run.txt";
constexpr const char* kUnsyncedName = "unsynced.csv";
constexpr size_t kRunDirMax = 24;   // "m004_2026-11-03" plus room for 4+ digit runs
constexpr size_t kLogPathMax = 48;  // "/log/m004_2026-11-03/2026-11-03.csv" is 35

struct RunInfo {
    int run = 0;                  // 0 = no run yet
    char dir[kRunDirMax] = "";    // folder name inside /log
    int64_t start_utc = 0;
    int64_t last_cal_utc = 0;     // at run start, 0 = never calibrated
    float temp_offset = 0.0f;     // at run start
};

// "m004_2026-11-03" from the run number and the local start date, or "m004_unsynced"
// when local_start is null (clock never synced).
void runDirName(int run, const Civil* local_start, char* buf, size_t len);
// "/log/<dir>/<name>".
void runFilePath(const char* dir, const char* name, char* buf, size_t len);
// "/log/<dir>/2026-11-03.csv" for the UTC day of utc.
void runDayLogPath(const char* dir, int64_t utc, char* buf, size_t len);
// Run number of a folder name like "m004_2026-11-03" (a leading path is ignored).
// -1 if the name is not a run folder.
int parseRunNumber(const char* name);
// Number of a new run: one above both the stored run and the highest run on the card,
// so a swapped card never gets a second folder with the same number.
inline int nextRunNumber(int stored_run, int max_on_card)
{
    return (stored_run > max_on_card ? stored_run : max_on_card) + 1;
}
// Day of the run, 1 on the start date, counted in local calendar days. The offsets are
// the local time offsets (s) at start and now, so a DST change does not shift the day.
int runDayIndex(int64_t start_utc, int64_t start_offset_s, int64_t now_utc, int64_t now_offset_s);
// Body of run.txt: run, start_utc, clock, firmware, last_cal_utc, temp_offset_c, one
// key=value per line. Later events (calibration=, clock_synced=) are appended lines.
size_t formatRunInfo(const RunInfo& info, const char* firmware, char* buf, size_t len);

} // namespace kf
