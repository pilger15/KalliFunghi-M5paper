/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "WifiList.h"

#include <string.h>

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int mergeNetworks(NetInfo* nets, int count)
{
    int n = 0;
    for (int i = 0; i < count; ++i) {
        if (nets[i].ssid[0] == '\0') { continue; }
        int j = 0;
        while (j < n && strcmp(nets[j].ssid, nets[i].ssid) != 0) { ++j; }
        if (j < n) {
            if (nets[i].rssi > nets[j].rssi) { nets[j] = nets[i]; }
            continue;
        }
        if (n != i) { nets[n] = nets[i]; }
        ++n;
    }
    // Insertion sort: at most a few dozen entries.
    for (int i = 1; i < n; ++i) {
        NetInfo v = nets[i];
        int j = i - 1;
        while (j >= 0 && (nets[j].rssi < v.rssi || (nets[j].rssi == v.rssi && strcmp(nets[j].ssid, v.ssid) > 0))) {
            nets[j + 1] = nets[j];
            --j;
        }
        nets[j + 1] = v;
    }
    return n;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
int signalBars(int rssi)
{
    if (rssi >= -55) { return 4; }
    if (rssi >= -65) { return 3; }
    if (rssi >= -75) { return 2; }
    if (rssi >= -85) { return 1; }
    return 0;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void printableSsid(const char* in, char* out, size_t out_len)
{
    if (out_len == 0) { return; }
    size_t i = 0;
    for (; in && in[i] && i + 1 < out_len; ++i) {
        unsigned char c = (unsigned char)in[i];
        out[i] = (c >= 0x20 && c <= 0x7E) ? (char)c : '?';
    }
    out[i] = '\0';
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int syncCandidates(const char* saved_ssid, const char* saved_pw, const char* def_ssid,
                   const char* def_pw, Credentials out[2])
{
    int n = 0;
    bool saved = saved_ssid && saved_ssid[0];
    if (saved) {
        out[n].ssid = saved_ssid;
        out[n].password = saved_pw ? saved_pw : "";
        ++n;
    }
    if (def_ssid && def_ssid[0] && !(saved && strcmp(saved_ssid, def_ssid) == 0)) {
        out[n].ssid = def_ssid;
        out[n].password = def_pw ? def_pw : "";
        ++n;
    }
    return n;
}

} // namespace kf
