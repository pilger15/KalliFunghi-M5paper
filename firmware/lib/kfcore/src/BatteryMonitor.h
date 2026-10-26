/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Low-battery decision and sidebar percent from the fixed-load-point voltage.
// M5Unified's own percent puts 0 % at 3.3 V (empty under load), so it is not used.
#pragma once

#include <stdint.h>

namespace kf {

class BatteryMonitor {
public:
    BatteryMonitor(int low_mv, int low_count, int empty_mv, int full_mv)
        : low_mv_(low_mv), low_count_(low_count), empty_mv_(empty_mv), full_mv_(full_mv) {}

    // Returns true once low_count consecutive readings are below low_mv.
    // Never true while on USB.
    bool feed(int mv, bool usb)
    {
        mv_ = mv;
        if (usb || mv >= low_mv_) {
            hits_ = 0;
            return false;
        }
        return ++hits_ >= low_count_;
    }

    int percent() const
    {
        if (mv_ <= empty_mv_) { return 0; }
        if (mv_ >= full_mv_) { return 100; }
        return (mv_ - empty_mv_) * 100 / (full_mv_ - empty_mv_);
    }
    int mv() const { return mv_; }

private:
    int low_mv_;
    int low_count_;
    int empty_mv_;
    int full_mv_;
    int mv_ = 0;
    int hits_ = 0;
};

} // namespace kf
