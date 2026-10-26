/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Tap vs double tap.
//
// Every tap opens a double-tap window (kDoubleTapWindowMs). A second tap inside
// the window is a DoubleTap, reported at once. A first tap on an actionable target
// is held back and reported as Tap when the window closes without a second tap.
// A first tap on a non-actionable target is never reported (it has no action),
// but it still counts as the first half of a double tap. So only taps that would
// do something wait, and a double tap never triggers the single-tap action first.
#pragma once

#include <stdint.h>

namespace kf {

struct TapEvent {
    enum Kind : uint8_t { None, Tap, DoubleTap };
    Kind kind = None;
    int16_t x = 0;
    int16_t y = 0;
};

class TapClassifier {
public:
    explicit TapClassifier(uint32_t window_ms) : window_ms_(window_ms) {}

    // A completed tap (finger released) at time now. Returns DoubleTap or None.
    TapEvent onTap(uint32_t now, int16_t x, int16_t y, bool actionable);
    // Returns the held-back Tap once its window has passed, else None.
    TapEvent poll(uint32_t now);
    bool windowOpen() const { return pending_; }
    uint32_t deadline() const { return t_ + window_ms_; }
    void reset() { pending_ = false; }

private:
    uint32_t window_ms_;
    bool pending_ = false;
    bool deferred_ = false;   // pending tap still has to be reported
    uint32_t t_ = 0;
    int16_t x_ = 0;
    int16_t y_ = 0;
};

} // namespace kf
