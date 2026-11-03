/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// App part for the Wi-Fi page and the on-screen keyboard: scan, connection test,
// saved network, keyboard input and its fast partial refreshes.
//
// Keyboard refresh: entering and leaving is one full GC16. Each key redraws only the
// changed region (text field, key area, OK key) with DU4 (epd_fastest) and returns
// without waiting for the waveform. While the panel still runs a waveform, changes
// are collected in kb_pending_ and pushed together once it is free, so the text is
// never drawn over a running update and the touch keeps being polled.
#include "App.h"

#include <M5Unified.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "hw/DebugTrace.h"
#include "net/WifiLink.h"

namespace kf {

namespace {

constexpr int kOkKeyIndex = 31;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
inline int32_t diff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool App::actionableAt(const UiShape& s, int x, int y) const
{
    Hit h = layout::hitTest(s, x, y);
    if (h.target == Target::None && view_ == View::Wifi) {
        return wifilayout::hitTest(wifi_page_.shape(), x, y).target != wifilayout::Target::None;
    }
    return layout::actionable(h);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void App::actAt(const UiShape& s, int x, int y, uint32_t now)
{
    Hit h = layout::hitTest(s, x, y);
    if (h.target == Target::None && view_ == View::Wifi) {
        wifiAct(wifilayout::hitTest(wifi_page_.shape(), x, y), now);
        return;
    }
    act(h, now);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void App::wifiStatus(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(wifi_page_.status, sizeof(wifi_page_.status), fmt, ap);
    va_end(ap);
    ++wifi_page_.version;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void App::updateWifiUses()
{
    char saved[33];
    char def[33];
    printableSsid(settings_.wifi_ssid, saved, sizeof(saved));
    printableSsid(wifi_.ssid, def, sizeof(def));
    Credentials nets[2];
    int n = syncCandidates(settings_.wifi_ssid, settings_.wifi_pass, wifi_.ssid, wifi_.password, nets);
    if (n == 2) { snprintf(wifi_page_.uses, sizeof(wifi_page_.uses), "Sync uses: %s, then hotspot %s", saved, def); }
    else if (saved[0]) { snprintf(wifi_page_.uses, sizeof(wifi_page_.uses), "Sync uses: %s", saved); }
    else { snprintf(wifi_page_.uses, sizeof(wifi_page_.uses), "Sync uses: hotspot %s (default)", def); }
    wifi_page_.can_forget = settings_.wifi_ssid[0] != '\0';
    ++wifi_page_.version;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void App::openWifiPage(uint32_t now)
{
    view_ = View::Wifi;
    wifi_page_.status[0] = '\0';
    wifi_page_.sync_now = false;
    updateWifiUses();
    markDirty(true, now);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void App::wifiAct(const wifilayout::Hit& h, uint32_t now)
{
    KF_TRACE("wifi target %d/%d", (int)h.target, h.index);
    switch (h.target) {
    case wifilayout::Target::Scan:
        wifiScan(now);
        break;
    case wifilayout::Target::EnterName:
        kb_hidden_ = true;
        kb_ssid_[0] = '\0';
        openKeyboard(KbPurpose::Ssid, now);
        return;
    case wifilayout::Target::Forget:
        settings_.forgetWifi();
        sd_.event("wifi saved network forgotten");
        wifi_page_.sync_now = false;
        updateWifiUses();
        wifiStatus("Saved network forgotten.");
        break;
    case wifilayout::Target::PageUp:
        if (wifi_page_.page > 0) { --wifi_page_.page; ++wifi_page_.version; }
        break;
    case wifilayout::Target::PageDown:
        if (wifi_page_.page + 1 < wifi_page_.pages()) { ++wifi_page_.page; ++wifi_page_.version; }
        break;
    case wifilayout::Target::SyncNow:
        wifi_page_.sync_now = false;
        view_ = View::Home;
        markDirty(true, now);
        runSync(true, now);
        return;
    case wifilayout::Target::Network: {
        const NetInfo& n = wifi_page_.nets[wifi_page_.page * wifilayout::kRowsPerPage + h.index];
        char name[33];
        printableSsid(n.ssid, name, sizeof(name));
        if (n.security == WifiSecurity::Enterprise) {
            wifiStatus("%s: 802.1X login not supported", name);
            break;
        }
        snprintf(kb_ssid_, sizeof(kb_ssid_), "%s", n.ssid);
        kb_hidden_ = false;
        if (n.security == WifiSecurity::Open) {
            wifiTest(kb_ssid_, "", now);
            return;
        }
        openKeyboard(KbPurpose::Password, now);
        return;
    }
    default:
        return;
    }
    markDirty(false, now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Same guards as the sync: enough battery, no calibration running, NVS marker and an
// error.log line before the radio starts (a Wi-Fi brown-out is then seen at boot).
const char* App::radioBegin(const char* what, uint32_t now)
{
    if (!psm_.usb() && battery_.mv() < kBatterySyncMinMv) { return "Battery too low for Wi-Fi"; }
    if (calibrating_) { return "Busy calibrating, try later"; }
    settings_.sync_running = true;
    settings_.save();
    sd_.event("wifi %s start, battery %d mV", what, battery_.mv());
    flush(now, true);
    psm_.onSyncStart();
    in_sync_ = true;
    return nullptr;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::radioEnd()
{
    in_sync_ = false;
    settings_.sync_running = false;
    settings_.save();
    psm_.onSyncEnd(millis());
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::wifiScan(uint32_t now)
{
    const char* why = radioBegin("scan", now);
    if (why) {
        wifiStatus("%s", why);
        markDirty(false, now);
        return;
    }
    wifi_page_.sync_now = false;
    wifiStatus("Scanning ...");
    markDirty(false, now);
    refresh(now);

    // Raw list larger than the shown one: duplicates (several access points per
    // SSID) are merged first, then the strongest kMaxNetworks are kept.
    static NetInfo raw[64];
    int n = wifilink::scan(raw, 64, kWifiScanTimeoutMs, [this]() { sample(millis()); });
    wifilink::off();
    radioEnd();
    now = millis();
    if (n < 0) {
        wifi_page_.count = 0;
        wifiStatus("Scan failed, try again.");
    } else {
        n = mergeNetworks(raw, n);
        if (n > kMaxNetworks) { n = kMaxNetworks; }
        memcpy(wifi_page_.nets, raw, sizeof(NetInfo) * (size_t)n);
        wifi_page_.count = n;
        if (n == 0) { wifiStatus("No networks found."); }
        else { wifiStatus("%d network%s. Tap one to connect.", n, n == 1 ? "" : "s"); }
    }
    wifi_page_.page = 0;
    wifi_page_.scanned = true;
    markDirty(false, now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Connects once, shows the result, Wi-Fi off. Only a good connection is saved.
void App::wifiTest(const char* ssid, const char* password, uint32_t now)
{
    char name[33];
    printableSsid(ssid, name, sizeof(name));
    wifi_page_.sync_now = false;
    const char* why = radioBegin("test", now);
    if (why) {
        wifiStatus("%s", why);
        markDirty(false, now);
        return;
    }
    wifiStatus("Connecting to %s ...", name);
    markDirty(false, now);
    refresh(now);

    wifilink::Result r = wifilink::connect(ssid, password, kWifiTestTimeoutMs, [this]() { sample(millis()); });
    wifilink::off();
    radioEnd();
    now = millis();
    sd_.event("wifi test %s: %s", ssid, wifilink::resultText(r));
    if (r == wifilink::Result::Ok) {
        settings_.saveWifi(ssid, password);
        updateWifiUses();
        wifi_page_.sync_now = true;
        wifiStatus("Connected to %s, saved.", name);
    } else {
        wifiStatus("%s: %s", name, wifilink::resultText(r));
    }
    markDirty(false, now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::openKeyboard(KbPurpose p, uint32_t now)
{
    kb_purpose_ = p;
    char name[33];
    printableSsid(kb_ssid_, name, sizeof(name));
    if (p == KbPurpose::Ssid) {
        kb_.begin(kb::Mode::Ssid);
        snprintf(kb_prompt_, sizeof(kb_prompt_), "Network name (hidden network)");
    } else {
        kb_.begin(kb_hidden_ ? kb::Mode::PasswordOrOpen : kb::Mode::Password);
        snprintf(kb_prompt_, sizeof(kb_prompt_), kb_hidden_ ? "Password for %s (empty = open)" : "Password for %s",
                 name);
    }
    // Write what is queued now, so the buffer does not fill while typing.
    flush(now, true);
    view_ = View::Keyboard;
    taps_.reset();
    touch_block_ = true;   // the finger that opened it is still down, or comes up now
    kb_last_key_ms_ = now;
    kb_next_tick_ms_ = now + kKbTickMs;
    kb_pending_ = 0;
    kb_press_key_ = -1;
    kb_hl_key_ = -1;
    kb_fast_updates_ = 0;
    kb_keys_ghosted_ = false;
    kb_t_wait_ = false;
    psm_.onTouch(now);

    uint32_t t0 = millis();
    display_.beginFrame(true);
    ui_.drawKeyboard(kb_, kb_prompt_);
    display_.endFrame();
    KF_TRACE("keyboard open (%d), GC16 %lu ms", (int)p, (unsigned long)(millis() - t0));
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::closeKeyboard(uint32_t now)
{
    view_ = View::Wifi;
    touch_block_ = true;
    kb_pending_ = 0;
    ++wifi_page_.version;
    markDirty(true, now);   // full GC16 of the Wi-Fi page
    KF_TRACE("keyboard closed");
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::keyDown(int x, int y, uint32_t now)
{
    if (!kbOpen()) { return; }
    kb::Key k = kb::hitTest(kb_.layer(), x, y);
    if (k.kind == kb::KeyKind::None) { return; }
    kb_last_key_ms_ = now;
    kb_t_key_ = millis();
    uint8_t ch = kb_.press(k, now);

    if (ch & kb::Keyboard::kCancel) {
        closeKeyboard(now);
        return;
    }
    if (ch & kb::Keyboard::kDone) {
        if (kb_purpose_ == KbPurpose::Ssid) {
            snprintf(kb_ssid_, sizeof(kb_ssid_), "%s", kb_.text());
            openKeyboard(KbPurpose::Password, now);
            return;
        }
        char pw[kb::kMaxText];
        snprintf(pw, sizeof(pw), "%s", kb_.text());
        closeKeyboard(now);
        wifiTest(kb_ssid_, pw, now);
        return;
    }
    if (kKbKeyHighlight && k.index >= 0) {
        kb_press_key_ = k.index;
        ch |= kb::Keyboard::kText;   // highlight rides on a push
    }
    if (ch == 0) { return; }
    kb_pending_ |= ch;
    pushKeyboard(now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Pushes the pending changes with DU4 if the panel is free; otherwise they stay
// pending and keyboardTick pushes them as soon as it is.
void App::pushKeyboard(uint32_t now)
{
    if (kb_pending_ == 0 || display_.busy()) { return; }
    uint8_t ch = kb_pending_;
    kb_pending_ = 0;
    uint32_t t0 = millis();

    display_.beginRegion(Display::Wave::Fastest);
    if (ch & (kb::Keyboard::kText | kb::Keyboard::kField)) {
        ui_.drawKbField(kb_, kb_prompt_);
        ++kb_fast_updates_;
    }
    if (ch & kb::Keyboard::kKeys) {
        ui_.drawKbKeys(kb_);
        kb_keys_ghosted_ = true;
        kb_hl_key_ = -1;
    } else if (ch & kb::Keyboard::kOkKey) {
        ui_.drawKbKey(kb_, kOkKeyIndex, false);
        kb_keys_ghosted_ = true;
    }
    if (kb_hl_key_ >= 0 && kb_hl_key_ != kb_press_key_) {
        ui_.drawKbKey(kb_, kb_hl_key_, false);
        kb_hl_key_ = -1;
    }
    if (kb_press_key_ >= 0) {
        ui_.drawKbKey(kb_, kb_press_key_, true);
        kb_hl_key_ = kb_press_key_;
        kb_hl_until_ = now + kKbHighlightMs;
        kb_press_key_ = -1;
        kb_keys_ghosted_ = true;
    }
    display_.endRegion(false);
    KF_TRACE("kb push 0x%02x: touch->sent %lu ms (draw+spi %lu ms)", (unsigned)ch,
             (unsigned long)(millis() - kb_t_key_), (unsigned long)(millis() - t0));
#if KF_TRACE_ENABLED
    kb_t_wait_ = true;
#endif
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Every pass while the keyboard is open: pending pushes, highlight restore, ghost
// cleanup, inactivity timeout.
void App::keyboardTick(uint32_t now)
{
    if (diff(now, kb_last_key_ms_) >= (int32_t)kKbTimeoutMs) {
        KF_TRACE("keyboard timeout");
        closeKeyboard(now);
        wifiStatus("Keyboard closed after 3 min without input.");
        return;
    }
#if KF_TRACE_ENABLED
    if (kb_t_wait_ && !display_.busy()) {
        kb_t_wait_ = false;
        KF_TRACE("kb waveform done: touch->done %lu ms", (unsigned long)(millis() - kb_t_key_));
    }
#endif
    if (kb_pending_) {
        pushKeyboard(now);
        return;
    }
    if (kb_hl_key_ >= 0 && diff(now, kb_hl_until_) >= 0) {
        if (display_.busy()) { return; }
        display_.beginRegion(Display::Wave::Fastest);
        ui_.drawKbKey(kb_, kb_hl_key_, false);
        display_.endRegion(false);
        kb_hl_key_ = -1;
        return;
    }
    // DU4 leaves faint ghosts: redraw with GC16 after kKbCleanKeys field updates, or
    // after a pause. Only the touched regions, and only when the panel is free.
    bool pause = diff(now, kb_last_key_ms_) >= (int32_t)kKbCleanPauseMs;
    bool clean = kb_fast_updates_ >= kKbCleanKeys || (pause && (kb_fast_updates_ > 0 || kb_keys_ghosted_));
    if (!clean || display_.busy()) { return; }
    uint32_t t0 = millis();
    if (kb_fast_updates_ > 0) {
        display_.beginRegion(Display::Wave::Quality);
        ui_.drawKbField(kb_, kb_prompt_);
        display_.endRegion(false);
    }
    if (pause && kb_keys_ghosted_) {
        display_.beginRegion(Display::Wave::Quality);
        ui_.drawKbKeys(kb_);
        display_.endRegion(false);
        kb_keys_ghosted_ = false;
    }
    KF_TRACE("kb cleanup after %d updates (%s), %lu ms", kb_fast_updates_, pause ? "pause" : "count",
             (unsigned long)(millis() - t0));
    kb_fast_updates_ = 0;
}

} // namespace kf
