/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Wires the modules together and owns the tick loop.
//
// One tick per wake:
//   battery at the fixed load point -> USB -> low battery -> RTC -> state machine
//   -> touch -> sensor mode -> sampling -> calibration -> auto-sync -> display
//   -> SD flush -> light sleep until the next deadline.
// All deadlines are millis() (esp_timer based, keeps running in light sleep).
// The RTC is only used for timestamps.
#pragma once

#include <stdint.h>
#include "BatteryMonitor.h"
#include "ColumnBins.h"
#include "Keyboard.h"
#include "Layout.h"
#include "PowerStateMachine.h"
#include "SampleRing.h"
#include "TapClassifier.h"
#include "Types.h"
#include "WifiLayout.h"
#include "power/UsbDetect.h"
#include "sensors/LightSensor.h"
#include "sensors/Scd41Sensor.h"
#include "storage/SdStore.h"
#include "storage/Settings.h"
#include "time/Clock.h"
#include "ui/Display.h"
#include "ui/Ui.h"

namespace kf {

class App {
public:
    App();
    void setup();
    void loop();

private:
    // Tick steps
    void handleTouch(uint32_t now);
    void act(const Hit& h, uint32_t now);
    void applySensorMode(uint32_t now);
    void sample(uint32_t now);
    void recordSample(bool scd_ok, const Scd41Sensor::Reading& r, bool lux_ok, float lux);
    void calibration(uint32_t now);
    void autoSync(uint32_t now);
    void refresh(uint32_t now);
    void flush(uint32_t now, bool force);
    void sleep(uint32_t now);

    // Helpers
    void runSync(bool manual, uint32_t now);
    bool startRun(uint32_t now, bool automatic);
    RunInfo runFromSettings() const;
    void lowBatteryShutdown();
    void setMessage(const char* msg, uint32_t now, uint32_t duration_ms);
    void markDirty(bool full, uint32_t now);
    void buildModel(UiModel& m, uint32_t now);
    void prepareBins(uint32_t now);
    int graphCols() const;
    int64_t utcNow() const;
    bool clockValid() const;
    bool calBoxVisible(int64_t utc) const;
    uint32_t sampleIntervalMs() const;
    void napOrDelay(uint32_t ms);
    void finishSample(bool scd_ok, const Scd41Sensor::Reading& r, uint32_t now);

    // Wi-Fi page and keyboard (AppWifi.cpp)
    enum class KbPurpose : uint8_t { Ssid, Password };
    bool actionableAt(const UiShape& s, int x, int y) const;
    void actAt(const UiShape& s, int x, int y, uint32_t now);
    void openWifiPage(uint32_t now);
    void wifiAct(const wifilayout::Hit& h, uint32_t now);
    void wifiStatus(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    void updateWifiUses();
    void wifiScan(uint32_t now);
    void wifiTest(const char* ssid, const char* password, uint32_t now);
    // Guards shared by scan, test and sync. Returns nullptr if the radio may start,
    // else the reason for the user.
    const char* radioBegin(const char* what, uint32_t now);
    void radioEnd();
    bool kbOpen() const { return view_ == View::Keyboard; }
    void openKeyboard(KbPurpose p, uint32_t now);
    void closeKeyboard(uint32_t now);
    void keyDown(int x, int y, uint32_t now);
    void keyboardTick(uint32_t now);
    void pushKeyboard(uint32_t now);

    Clock clock_;
    Settings settings_;
    SdStore sd_;
    Scd41Sensor scd_;
    LightSensor light_;
    UsbDetect usb_;
    BatteryMonitor battery_;
    PowerStateMachine psm_;
    TapClassifier taps_;
    Display display_;
    Ui ui_;
    WifiConfig wifi_;

    Sample* ring_storage_ = nullptr;
    SampleRing* ring_ = nullptr;
    ColumnBins* bins_[kFieldCount] = {nullptr, nullptr, nullptr, nullptr};

    // View
    View view_ = View::Home;
    int plot_ = 0;
    bool dirty_ = true;
    bool dirty_full_ = true;
    uint32_t last_signature_ = 0;
    uint32_t data_version_ = 0;
    PowerStateMachine::State last_state_ = PowerStateMachine::State::Boot;
    char message_[64] = "";
    uint32_t message_until_ = 0;

    // Bins cache for ranges longer than the RAM ring
    bool bins_cached_ = false;
    PlotRange bins_range_ = PlotRange::H6;
    int bins_cols_ = 0;
    int64_t bins_t1_ = 0;

    // Question in the bottom strip (new measurement, calibrate now)
    Ask ask_ = Ask::None;

    // Calibration
    bool cal_manual_open_ = false;
    bool calibrating_ = false;
    uint32_t cal_start_ms_ = 0;

    // Sampling
    uint32_t next_sample_ms_ = 0;
    uint32_t sample_wait_since_ = 0;
    bool waiting_for_scd_ = false;
    bool shot_pending_ = false;
    uint32_t shot_ready_ms_ = 0;
    int scd_missed_ = 0;
    uint32_t scd_reinits_ = 0;          // reinits since the last good reading
    uint32_t scd_reinit_logged_ms_ = 0;
    bool no_data_reported_ = false;   // "no sensor data" is in the error log already
    bool have_latest_ = false;
    Sample latest_;

    // Time and sync
    int64_t rtc_utc_ = 0;
    uint32_t rtc_ms_ = 0;
    uint32_t next_refresh_ms_ = 0;
    uint32_t last_flush_ms_ = 0;
    uint32_t next_autosync_ms_ = 0;
    bool in_sync_ = false;
    bool sync_reset_ = false;   // last run was reset during Wi-Fi: no auto-sync on battery this run

    // Wi-Fi page
    WifiPageModel wifi_page_;
    bool touch_block_ = false;          // ignore touches until all fingers are up (view switched under the finger)

    // Keyboard
    kb::Keyboard kb_;
    KbPurpose kb_purpose_ = KbPurpose::Password;
    bool kb_hidden_ = false;            // hidden-network flow: empty password = open network
    char kb_ssid_[33] = "";             // network the password is for
    char kb_prompt_[64] = "";
    uint32_t kb_last_key_ms_ = 0;
    uint32_t kb_next_tick_ms_ = 0;
    uint8_t kb_pending_ = 0;            // kb::Keyboard::Change bits not pushed yet (panel busy)
    int kb_press_key_ = -1;             // key to show pressed with the next push
    int kb_hl_key_ = -1;                // key shown pressed now
    uint32_t kb_hl_until_ = 0;
    int kb_fast_updates_ = 0;           // DU4 updates of the field since the last cleanup
    bool kb_keys_ghosted_ = false;      // key area got DU4 updates since the last cleanup
    uint32_t kb_t_key_ = 0;             // timing trace: touch-down seen
    bool kb_t_wait_ = false;            // timing trace: waiting for the waveform to end

    // VEML7700 read deferred instead of the 130 ms nap (keyboard open)
    bool lux_pending_ = false;
    uint32_t lux_ready_ms_ = 0;
    bool lux_scd_ok_ = false;
    Scd41Sensor::Reading lux_r_{};

    // Debug counters
    uint32_t wakes_ = 0;
    uint32_t samples_ = 0;
};

} // namespace kf
