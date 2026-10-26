/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "PowerStateMachine.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void PowerStateMachine::begin(uint32_t now, SamplingMode user_mode, bool usb)
{
    user_mode_ = user_mode;
    usb_ = usb;
    touch_t_ = now;
    // Boot counts as an interaction: the user just switched it on.
    state_ = usb ? State::UsbPerf : State::Interactive;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void PowerStateMachine::onTouch(uint32_t now)
{
    touch_t_ = now;
    if (state_ == State::Idle) { state_ = State::Interactive; }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void PowerStateMachine::onUsb(uint32_t now, bool present)
{
    if (state_ == State::LowBattOff) { return; }
    if (state_ == State::Syncing) {
        usb_pending_ = true;
        usb_pending_value_ = present;
        return;
    }
    if (present == usb_) { return; }
    usb_ = present;
    if (present) {
        state_ = State::UsbPerf;
    } else {
        // Fall back to the user's mode, idle (spec). The touch timer restarts only on touch.
        state_ = State::Idle;
        (void)now;
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void PowerStateMachine::onSyncStart()
{
    if (state_ == State::LowBattOff || state_ == State::Syncing) { return; }
    before_sync_ = state_;
    state_ = State::Syncing;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void PowerStateMachine::onSyncEnd(uint32_t now)
{
    if (state_ != State::Syncing) { return; }
    state_ = before_sync_;
    if (usb_pending_) {
        usb_pending_ = false;
        onUsb(now, usb_pending_value_);
    }
    update(now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void PowerStateMachine::update(uint32_t now)
{
    if (state_ == State::Interactive && (uint32_t)(now - touch_t_) >= p_.interactive_timeout_ms) {
        state_ = State::Idle;
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
SamplingMode PowerStateMachine::samplingMode() const
{
    return usb_ ? SamplingMode::Fast : user_mode_;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool PowerStateMachine::interactiveRefresh() const
{
    State s = (state_ == State::Syncing) ? before_sync_ : state_;
    return s == State::Interactive || s == State::UsbPerf;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
uint32_t PowerStateMachine::refreshIntervalMs() const
{
    if (!interactiveRefresh()) { return p_.refresh_idle_ms; }
    return (samplingMode() == SamplingMode::Fast) ? p_.refresh_interactive_fast_ms
                                                  : p_.refresh_interactive_slow_ms;
}

} // namespace kf
