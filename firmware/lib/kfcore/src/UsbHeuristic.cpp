/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "UsbHeuristic.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void UsbHeuristic::push(uint32_t t, int mv)
{
    if (n_ == kMaxPoints) {
        for (int i = 1; i < n_; ++i) { t_[i - 1] = t_[i]; mv_[i - 1] = mv_[i]; }
        --n_;
    }
    t_[n_] = t;
    mv_[n_] = mv;
    ++n_;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Keeps one point at or just beyond the window start, so "full window" can be judged.
void UsbHeuristic::dropOld(uint32_t now_ms)
{
    while (n_ >= 2 && (uint32_t)(now_ms - t_[1]) >= p_.window_ms) {
        for (int i = 1; i < n_; ++i) { t_[i - 1] = t_[i]; mv_[i - 1] = mv_[i]; }
        --n_;
    }
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool UsbHeuristic::windowFull(uint32_t now_ms) const
{
    return n_ >= 2 && (uint32_t)(now_ms - t_[0]) >= p_.window_ms;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool UsbHeuristic::feed(uint32_t now_ms, int mv)
{
    if (have_last_ && (uint32_t)(now_ms - last_point_) < p_.step_ms) { return usb_; }
    have_last_ = true;
    last_point_ = now_ms;
    push(now_ms, mv);
    dropOld(now_ms);

    int mn = mv_[0];
    int mx = mv_[0];
    for (int i = 1; i < n_; ++i) {
        if (mv_[i] < mn) { mn = mv_[i]; }
        if (mv_[i] > mx) { mx = mv_[i]; }
    }

    if (!usb_) {
        if (!windowFull(now_ms)) { return usb_; }
        bool flat_high = flat_allowed_ && mv >= p_.enter_mv && mv >= mx - 5;
        bool rising = n_ >= 3 && (mv - mn) >= p_.rise_mv
                      && mv_[n_ - 1] >= mv_[n_ - 2] && mv_[n_ - 2] >= mv_[n_ - 3];
        if (flat_high || rising) {
            usb_ = true;
            drop_hits_ = 0;
        }
        return usb_;
    }

    int prev = (n_ >= 2) ? mv_[n_ - 2] : mv;
    if (prev >= p_.leave_mv && mv < p_.leave_mv) {
        usb_ = false;
    } else if (mx - mv >= p_.drop_mv) {
        if (++drop_hits_ >= 2) { usb_ = false; }
    } else {
        drop_hits_ = 0;
    }
    if (!usb_) {
        // Fresh evidence needed: restart the window from this point.
        n_ = 0;
        push(now_ms, mv);
        drop_hits_ = 0;
        flat_allowed_ = false;
    }
    return usb_;
}

} // namespace kf
