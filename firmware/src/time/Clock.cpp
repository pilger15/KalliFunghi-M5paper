/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "Clock.h"

#include <M5Unified.h>
#include <stdlib.h>
#include <sys/time.h>
#include "TimeUtil.h"
#include "config.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Clock::begin()
{
    setenv("TZ", kTimezone, 1);
    tzset();
    volt_low_ = M5.Rtc.getVoltLow();
    readRtc();
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
int64_t Clock::readRtc()
{
    m5::rtc_datetime_t dt;
    if (!M5.Rtc.getDateTime(&dt)) { return utc_; }
    Civil c{dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes, dt.time.seconds};
    utc_ = utcFromCivil(c);
    struct timeval tv{(time_t)utc_, 0};
    settimeofday(&tv, nullptr);
    return utc_;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Clock::setUtc(int64_t utc)
{
    Civil c = civilFromUtc(utc);
    m5::rtc_datetime_t dt;
    dt.date.year = (int16_t)c.year;
    dt.date.month = (int8_t)c.month;
    dt.date.date = (int8_t)c.day;
    // Weekday: 1970-01-01 was a Thursday (4).
    int64_t days = (utc >= 0) ? utc / 86400 : (utc - 86399) / 86400;
    dt.date.weekDay = (int8_t)(((days % 7) + 11) % 7);
    dt.time.hours = (int8_t)c.hour;
    dt.time.minutes = (int8_t)c.minute;
    dt.time.seconds = (int8_t)c.second;
    M5.Rtc.setDateTime(dt);   // writing the time also clears the VL flag
    volt_low_ = false;
    utc_ = utc;
    struct timeval tv{(time_t)utc, 0};
    settimeofday(&tv, nullptr);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void Clock::local(int64_t utc, struct tm& out) const
{
    time_t t = (time_t)utc;
    localtime_r(&t, &out);
}

} // namespace kf
