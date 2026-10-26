/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
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
#include "Layout.h"
#include "PowerStateMachine.h"
#include "SampleRing.h"
#include "TapClassifier.h"
#include "Types.h"
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
    bool have_latest_ = false;
    Sample latest_;

    // Time and sync
    int64_t rtc_utc_ = 0;
    uint32_t rtc_ms_ = 0;
    uint32_t next_refresh_ms_ = 0;
    uint32_t last_flush_ms_ = 0;
    uint32_t next_autosync_ms_ = 0;
    bool in_sync_ = false;

    // Debug counters
    uint32_t wakes_ = 0;
    uint32_t samples_ = 0;
};

} // namespace kf
