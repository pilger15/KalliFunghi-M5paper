/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "WifiLink.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <string.h>
#include "hw/DebugTrace.h"

namespace kf {
namespace wifilink {

namespace {

volatile uint8_t g_last_reason = 0;

/* Moved by Claude Opus 5.5 from TimeSync.cpp | Review priority: Medium | Reviewed by: None */
Result resultFromReason(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        return Result::NotFound;
    // A wrong WPA2 password usually shows up as a handshake timeout, not AUTH_FAIL.
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return Result::WrongPassword;
    default:
        return Result::Failed;
    }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
WifiSecurity securityOf(wifi_auth_mode_t m)
{
    switch (m) {
    case WIFI_AUTH_OPEN:
    case WIFI_AUTH_OWE:
        return WifiSecurity::Open;
    case WIFI_AUTH_WPA2_ENTERPRISE:
    case WIFI_AUTH_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA2_WPA3_ENTERPRISE:
    case WIFI_AUTH_WPA3_ENT_192:
        return WifiSecurity::Enterprise;
    default:
        return WifiSecurity::Psk;
    }
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
const char* resultText(Result r)
{
    switch (r) {
    case Result::Ok: return "Connected";
    case Result::NotFound: return "Network not found";
    case Result::WrongPassword: return "Wrong password";
    case Result::Timeout: return "Connect timeout";
    default: return "Wi-Fi connect failed";
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void radioOn()
{
    if (WiFi.getMode() == WIFI_STA) { return; }
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
int scan(NetInfo* out, int max, uint32_t timeout_ms, const std::function<void()>& idle)
{
    radioOn();
    const uint32_t t0 = millis();
    if (WiFi.scanNetworks(true, false) == WIFI_SCAN_FAILED) { return -1; }
    int16_t n = WIFI_SCAN_RUNNING;
    while ((n = WiFi.scanComplete()) == WIFI_SCAN_RUNNING && millis() - t0 < timeout_ms) {
        idle();
        delay(100);
    }
    KF_TRACE("scan: %d entries, %lu ms", (int)n, (unsigned long)(millis() - t0));
    if (n < 0) {
        WiFi.scanDelete();
        return -1;
    }
    int count = 0;
    for (int i = 0; i < n && count < max; ++i) {
        NetInfo& e = out[count++];
        String s = WiFi.SSID(i);
        strncpy(e.ssid, s.c_str(), sizeof(e.ssid) - 1);
        e.ssid[sizeof(e.ssid) - 1] = '\0';
        int r = WiFi.RSSI(i);
        e.rssi = (int8_t)(r < -127 ? -127 : (r > 0 ? 0 : r));
        e.security = securityOf(WiFi.encryptionType(i));
    }
    WiFi.scanDelete();
    return count;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
Result connect(const char* ssid, const char* password, uint32_t timeout_ms,
               const std::function<void()>& idle)
{
    radioOn();
    const uint32_t t0 = millis();
    if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(false); }
    g_last_reason = 0;
    // ASSOC_LEAVE is our own disconnect from a previous attempt, not a reason to give up.
    WiFiEventId_t ev = WiFi.onEvent(
        [](WiFiEvent_t, WiFiEventInfo_t info) {
            uint8_t r = info.wifi_sta_disconnected.reason;
            if (r != WIFI_REASON_ASSOC_LEAVE) { g_last_reason = r; }
        },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.begin(ssid, (password && password[0]) ? password : nullptr);

    while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeout_ms) {
        if (g_last_reason != 0) { break; }
        idle();
        delay(100);
    }
    WiFi.removeEvent(ev);
    Result res = Result::Ok;
    if (WiFi.status() != WL_CONNECTED) {
        res = g_last_reason ? resultFromReason(g_last_reason) : Result::Timeout;
        // Stop the pending association, so the next attempt starts clean.
        WiFi.disconnect(false);
    }
    KF_TRACE("connect: result %d, reason %u, %lu ms", (int)res, (unsigned)g_last_reason,
             (unsigned long)(millis() - t0));
    return res;
}

/* Moved by Claude Opus 5.5 from TimeSync.cpp | Review priority: High | Reviewed by: None */
void off()
{
    if (esp_sntp_enabled()) { esp_sntp_stop(); }
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
}

} // namespace wifilink
} // namespace kf
