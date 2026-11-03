/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "RunLog.h"

#include <stdio.h>
#include <string.h>
#include "CsvFormat.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void runDirName(int run, const Civil* local_start, char* buf, size_t len)
{
    if (local_start) {
        snprintf(buf, len, "m%03d_%04d-%02d-%02d", run, local_start->year, local_start->month, local_start->day);
    } else {
        snprintf(buf, len, "m%03d_unsynced", run);
    }
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void runFilePath(const char* dir, const char* name, char* buf, size_t len)
{
    snprintf(buf, len, "%s/%s/%s", kLogDir, dir, name);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void runDayLogPath(const char* dir, int64_t utc, char* buf, size_t len)
{
    Civil c = civilFromUtc(utc);
    snprintf(buf, len, "%s/%s/%04d-%02d-%02d.csv", kLogDir, dir, c.year, c.month, c.day);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int parseRunNumber(const char* name)
{
    const char* slash = strrchr(name, '/');
    const char* p = slash ? slash + 1 : name;
    if (*p != 'm') { return -1; }
    ++p;
    int n = 0;
    int digits = 0;
    while (*p >= '0' && *p <= '9') {
        if (digits >= 6) { return -1; }
        n = n * 10 + (*p - '0');
        ++digits;
        ++p;
    }
    if (digits < 3 || *p != '_') { return -1; }
    return n;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int runDayIndex(int64_t start_utc, int64_t start_offset_s, int64_t now_utc, int64_t now_offset_s)
{
    int64_t d = utcDay(now_utc + now_offset_s) - utcDay(start_utc + start_offset_s) + 1;
    return d < 1 ? 1 : (int)d;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
size_t formatRunInfo(const RunInfo& info, const char* firmware, char* buf, size_t len)
{
    char start[24];
    formatIsoUtc(info.start_utc, start, sizeof(start));
    char cal[24] = "never";
    if (info.last_cal_utc != 0) { formatIsoUtc(info.last_cal_utc, cal, sizeof(cal)); }
    bool unsynced = strstr(info.dir, "unsynced") != nullptr;
    int n = snprintf(buf, len,
                     "run=%d\nstart_utc=%s\nclock=%s\nfirmware=%s\nlast_cal_utc=%s\ntemp_offset_c=%.1f\n",
                     info.run, start, unsynced ? "unsynced" : "synced", firmware, cal, (double)info.temp_offset);
    if (n < 0) { return 0; }
    return ((size_t)n < len) ? (size_t)n : len - 1;
}

} // namespace kf
