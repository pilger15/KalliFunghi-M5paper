/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// CSV log format. One file per UTC day: /log/YYYY-MM-DD.csv. Rows logged before
// the clock was ever synced go to /log/unsynced.csv with clock=unsynced.
// A missing sensor leaves its fields empty.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "Types.h"

namespace kf {

constexpr const char* kCsvHeader = "utc_iso,co2_ppm,temp_c,rh_pct,lux,mode,clock";
constexpr const char* kUnsyncedLogPath = "/log/unsynced.csv";
constexpr const char* kSyncLogPath = "/log/sync.csv";      // utc_iso,correction_s
constexpr size_t kCsvRowMax = 96;

// "2026-10-24T10:00:00Z"; buf needs 21 bytes.
void formatIsoUtc(int64_t utc, char* buf, size_t len);
// "/log/2026-10-24.csv"; buf needs 20 bytes.
void dayLogPath(int64_t utc, char* buf, size_t len);
// UTC day number (days since 1970), used to split batches at midnight.
inline int64_t utcDay(int64_t utc) { return (utc >= 0) ? utc / 86400 : (utc - 86399) / 86400; }
// One row without newline. Returns the length written.
size_t formatRow(const Sample& s, char* buf, size_t len);
// Parses a row written by formatRow. Returns false for the header or junk.
bool parseRow(const char* line, Sample& out);

} // namespace kf
