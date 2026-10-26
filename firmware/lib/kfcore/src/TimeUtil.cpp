/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "TimeUtil.h"

namespace kf {

namespace {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Days since 1970-01-01 for a proleptic Gregorian date (H. Hinnant's algorithm).
int64_t daysFromCivil(int y, int m, int d)
{
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
int64_t utcFromCivil(const Civil& c)
{
    return daysFromCivil(c.year, c.month, c.day) * 86400 + c.hour * 3600 + c.minute * 60 + c.second;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
Civil civilFromUtc(int64_t utc)
{
    int64_t days = utc / 86400;
    int64_t secs = utc % 86400;
    if (secs < 0) { secs += 86400; --days; }
    const int64_t z = days + 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    Civil c{};
    c.day = (int)(doy - (153 * mp + 2) / 5 + 1);
    c.month = (int)(mp < 10 ? mp + 3 : mp - 9);
    c.year = (int)(yoe + era * 400 + (c.month <= 2));
    c.hour = (int)(secs / 3600);
    c.minute = (int)(secs % 3600 / 60);
    c.second = (int)(secs % 60);
    return c;
}

} // namespace kf
