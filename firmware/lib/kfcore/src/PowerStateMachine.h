/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// The power state machine: the single place that decides sampling mode, display
// refresh interval and refresh waveform. Pure logic on monotonic milliseconds;
// the App feeds it events and asks it for intervals.
//
//                 touch                      timeout 5 min (no touch)
//     +------+  ---------->  +-------------+  ------------------------+
//     | IDLE |               | INTERACTIVE |                          |
//     +------+  <----------  +-------------+  <-- touch restarts timer |
//        ^  |      timeout          |                                 |
//        |  |                       |  USB present (any state)        |
//        |  +----------+------------+                                 |
//        |             v                                              |
//        |       +----------+                                         |
//        +-------| USB_PERF |  Fast sampling, interactive refresh,    |
//   USB gone     +----------+  no timeout                             |
//                                                                     |
//   SYNCING: entered from IDLE / INTERACTIVE / USB_PERF, returns to the state it
//            came from; a USB change during sync is applied when sync ends; touches
//            only extend the interactive timer.
//   LOW_BATT_OFF: entered from any state, terminal (App flushes, shows the final
//            screen, powers off).
//   BOOT: before begin().
//
// Sampling mode: USB_PERF -> Fast, otherwise the user setting (Ultra / Eco / Fast).
// Refresh interval: IDLE 5 min; INTERACTIVE 5 s (Fast) or 30 s (Eco, Ultra);
// USB_PERF 5 s. IDLE uses GC16 (full); interactive refreshes use DU, with a GC16
// every 10th partial and on every view change.
#pragma once

#include <stdint.h>
#include "Types.h"

namespace kf {

class PowerStateMachine {
public:
    enum class State : uint8_t { Boot, Idle, Interactive, UsbPerf, Syncing, LowBattOff };

    struct Params {
        uint32_t interactive_timeout_ms;
        uint32_t refresh_idle_ms;
        uint32_t refresh_interactive_fast_ms;
        uint32_t refresh_interactive_slow_ms;
    };

    explicit PowerStateMachine(const Params& p) : p_(p) {}

    void begin(uint32_t now, SamplingMode user_mode, bool usb);
    void setUserMode(SamplingMode m) { user_mode_ = m; }
    void onTouch(uint32_t now);
    void onUsb(uint32_t now, bool present);
    void onSyncStart();
    void onSyncEnd(uint32_t now);
    void onLowBattery() { state_ = State::LowBattOff; }
    // Applies the interactive timeout.
    void update(uint32_t now);

    State state() const { return state_; }
    bool usb() const { return usb_; }
    SamplingMode userMode() const { return user_mode_; }
    SamplingMode samplingMode() const;
    // Display refresh interval for the current state.
    uint32_t refreshIntervalMs() const;
    // True when refreshes are interactive (DU partials allowed).
    bool interactiveRefresh() const;
    // Time of the interactive timeout (valid in Interactive).
    uint32_t interactiveDeadline() const { return touch_t_ + p_.interactive_timeout_ms; }

private:
    Params p_;
    State state_ = State::Boot;
    State before_sync_ = State::Idle;
    SamplingMode user_mode_ = SamplingMode::Eco;
    bool usb_ = false;
    bool usb_pending_ = false;    // USB change seen during sync
    bool usb_pending_value_ = false;
    uint32_t touch_t_ = 0;
};

} // namespace kf
