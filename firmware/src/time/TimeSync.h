/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Connect & Sync: first network that connects (saved network, then the default
// hotspot) -> NTP -> BM8563, then Wi-Fi fully off. The radio stays on between the
// attempts (one power-up peak). Each attempt (connect plus NTP) is limited to
// timeout_ms; idle() is called about every 100 ms so the App keeps sampling.
#pragma once

#include <functional>
#include <stdint.h>
#include "WifiList.h"
#include "net/WifiLink.h"

namespace kf {

class Clock;

struct SyncResult {
    enum Code : uint8_t { Ok, HotspotNotFound, WrongPassword, ConnectFailed, NtpTimeout, ConnectTimeout };
    Code code = ConnectFailed;   // of the last attempt
    int64_t correction_s = 0;    // NTP minus RTC, valid for Ok
    int64_t utc = 0;             // time after the sync, valid for Ok
    int attempts = 0;            // networks tried
    int used = -1;               // index of the network that connected, -1 = none
    wifilink::Result link[2] = {wifilink::Result::Failed, wifilink::Result::Failed};
};

const char* syncResultText(SyncResult::Code c);

SyncResult runTimeSync(Clock& clock, const Credentials* nets, int count, uint32_t timeout_ms,
                       const std::function<void()>& idle);

} // namespace kf
