/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Wall clock. The BM8563 holds UTC and is read on every wake (the ESP32 sleep
// clock drifts too much). Local time only for display (TZ CET/CEST).
// Scheduling never uses this clock: it jumps on sync. See App (millis / esp_timer).
#pragma once

#include <stdint.h>
#include <time.h>

namespace kf {

class Clock {
public:
    void begin();
    // Reads the BM8563 and updates the system time. Returns UTC seconds.
    int64_t readRtc();
    int64_t utc() const { return utc_; }
    // BM8563 VL flag: the oscillator stopped, time is invalid until written.
    bool voltLow() const { return volt_low_; }
    // Writes UTC to the BM8563 and the system time.
    void setUtc(int64_t utc);
    // Local broken-down time for display.
    void local(int64_t utc, struct tm& out) const;

private:
    int64_t utc_ = 0;
    bool volt_low_ = false;
};

} // namespace kf
