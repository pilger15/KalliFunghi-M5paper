/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Wi-Fi scan results and the network order for sync. Pure (no Arduino), unit tested.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace kf {

enum class WifiSecurity : uint8_t { Open, Psk, Enterprise };

struct NetInfo {
    char ssid[33] = "";
    int8_t rssi = -127;
    WifiSecurity security = WifiSecurity::Open;
};

constexpr int kMaxNetworks = 30;

// In place: drops hidden entries (empty SSID), merges equal SSIDs (the strongest
// entry wins, with its security), sorts by RSSI (strongest first), ties by SSID.
// Returns the new count.
int mergeNetworks(NetInfo* nets, int count);

// 0..4 bars from the RSSI in dBm.
int signalBars(int rssi);

// Copy of an SSID for display: bytes outside printable ASCII become '?'.
void printableSsid(const char* in, char* out, size_t out_len);

struct Credentials {
    const char* ssid = nullptr;
    const char* password = nullptr;
};

// Networks for a sync, in the order to try: the saved network (if any), then the
// default hotspot (config.json over secrets.h, resolved by the caller). The default
// is left out when it has the same SSID as the saved one. Returns the count (0..2).
int syncCandidates(const char* saved_ssid, const char* saved_pw, const char* def_ssid,
                   const char* def_pw, Credentials out[2]);

} // namespace kf
