/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Connect & Sync: phone hotspot -> NTP -> BM8563, then Wi-Fi fully off.
// Blocks for at most the timeout; calls idle() about every 100 ms so the App can
// keep sampling while it waits.
#pragma once

#include <functional>
#include <stdint.h>

namespace kf {

class Clock;

struct SyncResult {
    enum Code : uint8_t { Ok, HotspotNotFound, WrongPassword, ConnectFailed, NtpTimeout };
    Code code = ConnectFailed;
    int64_t correction_s = 0;    // NTP minus RTC, valid for Ok
    int64_t utc = 0;             // time after the sync, valid for Ok
};

const char* syncResultText(SyncResult::Code c);

SyncResult runTimeSync(Clock& clock, const char* ssid, const char* password,
                       uint32_t timeout_ms, const std::function<void()>& idle);

} // namespace kf
