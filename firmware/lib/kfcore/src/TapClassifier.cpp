/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "TapClassifier.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
TapEvent TapClassifier::onTap(uint32_t now, int16_t x, int16_t y, bool actionable)
{
    TapEvent ev;
    if (pending_ && (uint32_t)(now - t_) <= window_ms_) {
        pending_ = false;
        ev.kind = TapEvent::DoubleTap;
        ev.x = x;
        ev.y = y;
        return ev;
    }
    pending_ = true;
    deferred_ = actionable;
    t_ = now;
    x_ = x;
    y_ = y;
    return ev;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
TapEvent TapClassifier::poll(uint32_t now)
{
    TapEvent ev;
    if (pending_ && (uint32_t)(now - t_) > window_ms_) {
        pending_ = false;
        if (deferred_) {
            ev.kind = TapEvent::Tap;
            ev.x = x_;
            ev.y = y_;
        }
    }
    return ev;
}

} // namespace kf
