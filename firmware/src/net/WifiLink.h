/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Wi-Fi radio: scan and connect for the Wi-Fi page and the sync. The caller turns
// the radio off with off() when done (scan, test and sync each end with Wi-Fi fully
// off). Every wait calls idle() about every 100 ms, so the App keeps sampling.
#pragma once

#include <functional>
#include <stdint.h>
#include "WifiList.h"

namespace kf {
namespace wifilink {

enum class Result : uint8_t { Ok, NotFound, WrongPassword, Timeout, Failed };

const char* resultText(Result r);

// Station mode, no persistence, no auto-reconnect. Called by scan() and connect().
void radioOn();
// Async scan, waits for it (at most timeout_ms). Returns the number of entries in out
// (raw, not merged), or -1 if the scan failed.
int scan(NetInfo* out, int max, uint32_t timeout_ms, const std::function<void()>& idle);
// Connects and waits until connected, a disconnect reason, or the timeout. Leaves
// the radio on (connected or not); a second connect() reuses it.
Result connect(const char* ssid, const char* password, uint32_t timeout_ms,
               const std::function<void()>& idle);
// SNTP stopped, disconnected, radio off.
void off();

} // namespace wifilink
} // namespace kf
