/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// UTC calendar conversion without timegm (newlib has none). Pure, unit tested.
#pragma once

#include <stdint.h>

namespace kf {

struct Civil {
    int year;
    int month;   // 1..12
    int day;     // 1..31
    int hour;
    int minute;
    int second;
};

int64_t utcFromCivil(const Civil& c);
Civil civilFromUtc(int64_t utc);

} // namespace kf
