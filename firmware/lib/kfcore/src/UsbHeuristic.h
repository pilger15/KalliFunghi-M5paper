/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// USB detection from the battery voltage, for when there is no hardware signal.
//
// One voltage point per step (default 1 min), always taken at the same load point
// (right after wake, radio and EPD off). Points older than the window (10 min) drop out.
//
// Battery -> USB, only with a full window of points:
//   - charging:  newest - window min >= rise (30 mV) and the last 3 points non-decreasing.
//   - flat high: newest >= enter (4.15 V) and newest >= window max - 5 mV. Only until
//     the first USB -> battery exit (i.e. for a device booted on USB with a full cell):
//     a just-unplugged full cell also sits flat above 4.15 V and must not re-enter.
// USB -> battery (history restarts from the exit point):
//   - crossing: previous point >= leave (4.10 V) and newest < leave, or
//   - drop:     window max - newest >= drop (20 mV) on 2 points in a row (the step from
//               the charge voltage to the loaded cell voltage when USB is pulled).
// "Below 4.10 V" is a crossing, not a level: charging from a low cell runs below 4.10 V.
//
// Limits (why the bench run and the GPIO3 / VBUS options matter):
//   - Plug-in is seen after one full window (about 10 min).
//   - Unplug is seen after 2 points (2 min) if the voltage steps down by 20 mV or more.
//     A smaller step is only caught by the 4.10 V crossing: Fast drain (about 20 mA)
//     until then, worst case several hours for a full cell.
//   - Re-plugging a nearly full cell (above about 4.17 V) may give less than 30 mV rise
//     and go unnoticed until the cell has discharged further.
//   - No time cap in USB: with a full cell the charger holds the voltage constant at
//     any load, so a re-check cannot tell USB from battery.
#pragma once

#include <stdint.h>

namespace kf {

class UsbHeuristic {
public:
    struct Params {
        int enter_mv;
        int leave_mv;
        int rise_mv;
        int drop_mv;
        uint32_t window_ms;
        uint32_t step_ms;
    };

    explicit UsbHeuristic(const Params& p) : p_(p) {}

    // Feed the fixed-load-point battery voltage. Returns the current USB state.
    bool feed(uint32_t now_ms, int mv);
    bool usb() const { return usb_; }

private:
    static constexpr int kMaxPoints = 32;
    void push(uint32_t t, int mv);
    void dropOld(uint32_t now_ms);
    bool windowFull(uint32_t now_ms) const;

    Params p_;
    bool usb_ = false;
    bool flat_allowed_ = true;   // flat-high entry only until the first exit
    int drop_hits_ = 0;
    bool have_last_ = false;
    uint32_t last_point_ = 0;
    uint32_t t_[kMaxPoints];
    int mv_[kMaxPoints];
    int n_ = 0;
};

} // namespace kf
