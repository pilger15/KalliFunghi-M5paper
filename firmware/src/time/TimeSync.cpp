/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "TimeSync.h"

#include <Arduino.h>
#include <esp_sntp.h>
#include <sys/time.h>
#include "Clock.h"
#include "config.h"
#include "hw/DebugTrace.h"

namespace kf {

namespace {

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
int64_t systemUtc()
{
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    return (int64_t)tv.tv_sec;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
SyncResult::Code codeFromLink(wifilink::Result r)
{
    switch (r) {
    case wifilink::Result::NotFound: return SyncResult::HotspotNotFound;
    case wifilink::Result::WrongPassword: return SyncResult::WrongPassword;
    case wifilink::Result::Timeout: return SyncResult::ConnectTimeout;
    default: return SyncResult::ConnectFailed;
    }
}

} // namespace

/* Edited by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
const char* syncResultText(SyncResult::Code c)
{
    switch (c) {
    case SyncResult::Ok: return "Sync ok";
    case SyncResult::HotspotNotFound: return "Network not found";
    case SyncResult::WrongPassword: return "Wrong password";
    case SyncResult::NtpTimeout: return "NTP timeout";
    case SyncResult::ConnectTimeout: return "Connect timeout";
    default: return "Wi-Fi connect failed";
    }
}

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
SyncResult runTimeSync(Clock& clock, const Credentials* nets, int count, uint32_t timeout_ms,
                       const std::function<void()>& idle)
{
    SyncResult res;
    // Reference: RTC time at a known millis(), to measure the correction.
    const int64_t rtc_ref = clock.readRtc();
    const uint32_t ms_ref = millis();

    uint32_t t_start = millis();
    for (int i = 0; i < count && i < 2; ++i) {
        t_start = millis();
        res.attempts = i + 1;
        res.link[i] = wifilink::connect(nets[i].ssid, nets[i].password, timeout_ms, idle);
        if (res.link[i] == wifilink::Result::Ok) {
            res.used = i;
            break;
        }
        res.code = codeFromLink(res.link[i]);
    }
    if (res.used < 0) {
        KF_TRACE("sync: no connection after %d attempts", res.attempts);
        wifilink::off();
        return res;
    }

    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, kNtpServer1);
    esp_sntp_setservername(1, kNtpServer2);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_sync_status(SNTP_SYNC_STATUS_RESET);
    esp_sntp_init();

    // NTP shares the time budget of the attempt that connected.
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
    KF_TRACE("sync: code %d corr %lld via %d", (int)res.code, (long long)res.correction_s, res.used);
    wifilink::off();
    return res;
}

} // namespace kf
