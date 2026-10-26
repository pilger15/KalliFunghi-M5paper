/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "TimeSync.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "Clock.h"
#include "config.h"
#include "hw/DebugTrace.h"

namespace kf {

namespace {

volatile uint8_t g_last_reason = 0;

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
SyncResult::Code codeFromReason(uint8_t reason)
{
    switch (reason) {
    case WIFI_REASON_NO_AP_FOUND:
    case WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY:
    case WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD:
        return SyncResult::HotspotNotFound;
    // A wrong WPA2 password usually shows up as a handshake timeout, not AUTH_FAIL.
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
        return SyncResult::WrongPassword;
    default:
        return SyncResult::ConnectFailed;
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void wifiOff()
{
    if (esp_sntp_enabled()) { esp_sntp_stop(); }
    WiFi.disconnect(true, true);
    WiFi.mode(WIFI_OFF);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
int64_t systemUtc()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (int64_t)tv.tv_sec;
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
const char* syncResultText(SyncResult::Code c)
{
    switch (c) {
    case SyncResult::Ok: return "Sync ok";
    case SyncResult::HotspotNotFound: return "Hotspot not found";
    case SyncResult::WrongPassword: return "Wrong password";
    case SyncResult::NtpTimeout: return "NTP timeout";
    default: return "Wi-Fi connect failed";
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
SyncResult runTimeSync(Clock& clock, const char* ssid, const char* password,
                       uint32_t timeout_ms, const std::function<void()>& idle)
{
    SyncResult res;
    const uint32_t t_start = millis();
    // Reference: RTC time at a known millis(), to measure the correction.
    const int64_t rtc_ref = clock.readRtc();
    const uint32_t ms_ref = millis();

    g_last_reason = 0;
    WiFiEventId_t ev = WiFi.onEvent(
        [](WiFiEvent_t, WiFiEventInfo_t info) { g_last_reason = info.wifi_sta_disconnected.reason; },
        ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED && millis() - t_start < timeout_ms) {
        if (g_last_reason != 0) { break; }
        idle();
        delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
        res.code = g_last_reason ? codeFromReason(g_last_reason) : SyncResult::HotspotNotFound;
        KF_TRACE("sync: no connection, reason %u", (unsigned)g_last_reason);
        WiFi.removeEvent(ev);
        wifiOff();
        return res;
    }

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, kNtpServer1);
    esp_sntp_setservername(1, kNtpServer2);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    esp_sntp_init();

    res.code = SyncResult::NtpTimeout;
    while (millis() - t_start < timeout_ms) {
        if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
            int64_t ntp = systemUtc();
            int64_t expected = rtc_ref + (int64_t)(millis() - ms_ref) / 1000;
            res.code = SyncResult::Ok;
            res.correction_s = ntp - expected;
            res.utc = ntp;
            clock.setUtc(ntp);
            break;
        }
        idle();
        delay(100);
    }
    KF_TRACE("sync: code %d corr %lld", (int)res.code, (long long)res.correction_s);
    WiFi.removeEvent(ev);
    wifiOff();
    return res;
}

} // namespace kf
