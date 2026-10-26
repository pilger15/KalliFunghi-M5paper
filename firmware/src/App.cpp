/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "App.h"

#include <M5Unified.h>
#include <SPI.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <new>
#include <stdio.h>
#include <string.h>
#include "CsvFormat.h"
#include "config.h"
#include "hw/Board.h"
#include "hw/DebugTrace.h"
#include "time/TimeSync.h"

namespace kf {

namespace {

const PowerStateMachine::Params kPsmParams{
    kInteractiveTimeoutMs, kRefreshIdleMs, kRefreshInteractiveFastMs, kRefreshInteractiveEcoMs,
};

constexpr uint32_t kMessageMs = 2UL * 60 * 1000;
constexpr uint32_t kAutoSyncFirstMs = 5UL * 60 * 1000;
constexpr uint32_t kMaxSleepMs = 60UL * 60 * 1000;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// Signed difference a - b for wrapping millis() values.
inline int32_t diff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
template <typename T>
T* allocPsram(size_t count)
{
    void* p = heap_caps_malloc(sizeof(T) * count, MALLOC_CAP_SPIRAM);
    if (!p) { p = malloc(sizeof(T) * count); }
    return static_cast<T*>(p);
}

} // namespace

App::App()
    : battery_(kBatteryLowMv, kBatteryLowCount, kBatteryEmptyMv, kBatteryFullMv),
      psm_(kPsmParams),
      taps_(kDoubleTapWindowMs)
{
    snprintf(wifi_.ssid, sizeof(wifi_.ssid), "%s", kDefaultSsid);
    snprintf(wifi_.password, sizeof(wifi_.password), "%s", kDefaultPassword);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::setup()
{
    board::earlyInit();
    KF_TRACE_BEGIN();

    auto cfg = M5.config();
    cfg.serial_baudrate = 0;     // UART0 pins belong to USB detection
    cfg.clear_display = false;   // keep the last image (low-battery screen) until we draw
    cfg.output_power = true;     // EXT5V on: SCD41 and VEML7700 run from it
    cfg.internal_imu = false;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    cfg.external_imu = false;
    cfg.external_rtc = false;
    M5.begin(cfg);
    usb_.begin();

    settings_.load();
    clock_.begin();
    rtc_utc_ = clock_.utc();
    rtc_ms_ = millis();

    // After a low-battery power-off, only start once the cell has recovered (hysteresis).
    int mv = M5.Power.getBatteryVoltage();
    if (settings_.low_batt_off) {
        if (mv < kBatteryBootMinMv) {
            KF_TRACE("boot: battery %d mV still low, off", mv);
            board::powerOff();
        }
        settings_.low_batt_off = false;
        settings_.save();
    }

    display_.begin();
    // After the low-battery check, so a too-low restart keeps the "Battery low" screen.
    display_.clean(kBootCleanCycles);
    SPI.begin(kPinSpiSck, kPinSpiMiso, kPinSpiMosi, kPinSdCs);
    sd_.mount();
    sd_.loadConfig(wifi_);

    ring_storage_ = allocPsram<Sample>(kRingCapacity);
    ring_ = new SampleRing(ring_storage_, ring_storage_ ? kRingCapacity : 0);
    for (int f = 0; f < kFieldCount; ++f) {
        bins_[f] = new (allocPsram<ColumnBins>(1)) ColumnBins();
    }
    if (clockValid()) { sd_.loadRing(*ring_, rtc_utc_, kRingSeconds); }
    if (!ring_->empty()) {
        latest_ = ring_->newest();
        have_latest_ = true;
    }

    Wire.begin(kPinPortASda, kPinPortAScl, kPortAI2cHz);
    delay(50);   // SCD41 power-up time after EXT5V
    scd_.begin(Wire, settings_.temp_offset, !settings_.asc_off_persisted);
    if (scd_.ascPersistedNow() && !settings_.asc_off_persisted) {
        settings_.asc_off_persisted = true;
        settings_.save();
    }
    light_.begin(Wire);

    if (settings_.cal.last_cal_utc == 0) {
        // No reminder on day one.
        settings_.cal.calibrated(rtc_utc_);
        settings_.save();
    }

    uint32_t now = millis();
    psm_.begin(now, settings_.mode, false);
    last_state_ = psm_.state();
    scd_.setMode(psm_.samplingMode(), now);
    next_sample_ms_ = now + 1000;
    next_refresh_ms_ = now;
    last_flush_ms_ = now;
    next_autosync_ms_ = now + kAutoSyncFirstMs;
    markDirty(true, now);
    KF_TRACE("boot ok, utc %lld, valid %d, scd %d, veml %d, sd %d", (long long)rtc_utc_,
             (int)clockValid(), (int)scd_.present(), (int)light_.present(), (int)sd_.mounted());
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::loop()
{
    uint32_t now = millis();
    ++wakes_;

    // Fixed load point: radio off, EPD in standby or off, before any work.
    int mv = M5.Power.getBatteryVoltage();
    bool usb = usb_.update(now, mv);
    if (battery_.feed(mv, usb)) { lowBatteryShutdown(); }

    rtc_utc_ = clock_.readRtc();
    rtc_ms_ = millis();

    psm_.onUsb(now, usb);
    psm_.update(now);
    if (psm_.state() != last_state_) {
        // Interactive marker, mode or plug mark changed: show it now.
        KF_TRACE("state %d -> %d", (int)last_state_, (int)psm_.state());
        last_state_ = psm_.state();
        markDirty(false, now);
    }
    if (message_[0] && diff(now, message_until_) >= 0) {
        message_[0] = '\0';
        markDirty(false, now);
    }

    handleTouch(now);
    applySensorMode(now);
    sample(now);
    calibration(now);
    autoSync(now);
    refresh(now);
    flush(now, false);
    sleep(now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::handleTouch(uint32_t now)
{
    M5.update();
    UiModel shape_model;
    shape_model.view = view_;
    shape_model.box = calBoxVisible(utcNow()) || (message_[0] != '\0');
    shape_model.box_buttons = calBoxVisible(utcNow());
    shape_model.cal_manual = cal_manual_open_;
    shape_model.calibrating = calibrating_;
    UiShape shape = Ui::shape(shape_model);

    for (int i = 0; i < M5.Touch.getCount(); ++i) {
        const auto& t = M5.Touch.getDetail(i);
        if (t.wasPressed()) { psm_.onTouch(now); }
        if (t.wasClicked()) {
            Hit h = layout::hitTest(shape, t.x, t.y);
            TapEvent ev = taps_.onTap(now, t.x, t.y, layout::actionable(h));
            if (ev.kind == TapEvent::DoubleTap) {
                KF_TRACE("double tap");
                if (view_ != View::Home) {
                    view_ = View::Home;
                    markDirty(true, now);
                }
            }
        }
    }
    TapEvent ev = taps_.poll(now);
    if (ev.kind == TapEvent::Tap) {
        act(layout::hitTest(shape, ev.x, ev.y), now);
    }
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void App::act(const Hit& h, uint32_t now)
{
    KF_TRACE("tap target %d/%d", (int)h.target, h.index);
    switch (h.target) {
    case Target::PlotTile:
        view_ = View::Plot;
        plot_ = h.index;
        markDirty(true, now);
        break;
    case Target::Sync:
        runSync(true, now);
        break;
    case Target::Settings:
        view_ = (view_ == View::Settings) ? View::Home : View::Settings;
        markDirty(true, now);
        break;
    case Target::Range:
        settings_.range = (PlotRange)h.index;
        settings_.save();
        bins_cached_ = false;
        markDirty(false, now);
        break;
    case Target::Mode:
        settings_.mode = (SamplingMode)h.index;
        settings_.save();
        psm_.setUserMode(settings_.mode);
        markDirty(false, now);
        break;
    case Target::AutoSync:
        settings_.auto_sync = !settings_.auto_sync;
        settings_.save();
        markDirty(false, now);
        break;
    case Target::OffsetMinus:
    case Target::OffsetPlus: {
        float o = settings_.temp_offset + (h.target == Target::OffsetPlus ? kTempOffsetStep : -kTempOffsetStep);
        if (o < kTempOffsetMin) { o = kTempOffsetMin; }
        if (o > kTempOffsetMax) { o = kTempOffsetMax; }
        if (o != settings_.temp_offset) {
            settings_.temp_offset = o;
            settings_.save();
            scd_.setTemperatureOffset(o, now);
            waiting_for_scd_ = false;
            next_sample_ms_ = now + sampleIntervalMs();
        }
        markDirty(false, now);
        break;
    }
    case Target::CalibrateNow:
        if (!calibrating_) {
            cal_manual_open_ = true;
            message_[0] = '\0';
            view_ = View::Home;
            markDirty(true, now);
        }
        break;
    case Target::CalConfirm:
        if (!calibrating_) {
            calibrating_ = true;
            cal_start_ms_ = now;
            message_[0] = '\0';
            if (scd_.mode() == SamplingMode::Ultra) { next_sample_ms_ = now; }
            KF_TRACE("calibration started");
            markDirty(false, now);
        }
        break;
    case Target::CalPostpone:
        settings_.cal.postpone(utcNow(), kCalSnoozeS);
        settings_.save();
        markDirty(false, now);
        break;
    case Target::CalCancel:
        cal_manual_open_ = false;
        calibrating_ = false;
        markDirty(false, now);
        break;
    default:
        break;
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::applySensorMode(uint32_t now)
{
    SamplingMode want = psm_.samplingMode();
    if (want == scd_.mode()) { return; }
    // Never switch during a calibration: it would reset the 3 min warm-up.
    if (calibrating_) { return; }
    KF_TRACE("sensor mode %d -> %d", (int)scd_.mode(), (int)want);
    flush(now, true);
    scd_.setMode(want, now);
    waiting_for_scd_ = false;
    shot_pending_ = false;
    scd_missed_ = 0;
    // First reading after the change is discarded inside Scd41Sensor.
    next_sample_ms_ = now + ((want == SamplingMode::Ultra) ? 0 : sampleIntervalMs());
    markDirty(false, now);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
uint32_t App::sampleIntervalMs() const
{
    switch (scd_.mode()) {
    case SamplingMode::Fast: return kSampleFastMs;
    case SamplingMode::Ultra: return calibrating_ ? kScdSingleShotMs : kSampleUltraMs;
    default: return kSampleEcoMs;
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Light sleep is not allowed while Wi-Fi is up (sync); plain delay then.
void App::napOrDelay(uint32_t ms)
{
    if (in_sync_) { delay(ms); }
    else { board::nap(ms); }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::sample(uint32_t now)
{
    Scd41Sensor::Reading r{};

    if (scd_.mode() == SamplingMode::Ultra) {
        if (shot_pending_) {
            if (diff(now, shot_ready_ms_) < 0) { return; }
            shot_pending_ = false;
            bool ok = scd_.readShot(r);
            light_.start();
            napOrDelay(kVemlSettleMs);
            float lux = 0.0f;
            bool lux_ok = light_.read(lux);
            recordSample(ok, r, lux_ok, lux);
            return;
        }
        if (diff(now, next_sample_ms_) < 0) { return; }
        next_sample_ms_ = now + sampleIntervalMs();
        if (!scd_.present() && !scd_.reinit(now)) {
            // Sensor missing: still log light on the Ultra cycle.
            light_.start();
            napOrDelay(kVemlSettleMs);
            float lux = 0.0f;
            bool lux_ok = light_.read(lux);
            recordSample(false, r, lux_ok, lux);
            return;
        }
        if (scd_.triggerShot()) {
            shot_pending_ = true;
            shot_ready_ms_ = now + kScdSingleShotMs;
        }
        return;
    }

    if (diff(now, next_sample_ms_) < 0) { return; }
    if (!waiting_for_scd_) {
        waiting_for_scd_ = true;
        sample_wait_since_ = now;
    }
    bool ok = scd_.poll(r);
    if (!ok && scd_.present() && !scd_.error()
        && diff(now, sample_wait_since_) < (int32_t)kScdDataReadyMaxWaitMs) {
        // Sensor cycle not done yet: come back in a second.
        next_sample_ms_ = now + kScdDataReadyRetryMs;
        return;
    }
    waiting_for_scd_ = false;
    next_sample_ms_ = now + sampleIntervalMs();
    light_.start();
    napOrDelay(kVemlSettleMs);
    float lux = 0.0f;
    bool lux_ok = light_.read(lux);
    recordSample(ok, r, lux_ok, lux);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::recordSample(bool scd_ok, const Scd41Sensor::Reading& r, bool lux_ok, float lux)
{
    uint32_t now = millis();
    if (scd_ok) {
        scd_missed_ = 0;
    } else if (++scd_missed_ >= kScdMissedIntervalsReinit) {
        // No data for 3 intervals (unplugged, brown-out, I2C error): re-init, which also
        // re-applies ASC off and the offset a brown-out would have lost.
        scd_missed_ = 0;
        if (!calibrating_) { scd_.reinit(now); }
        next_sample_ms_ = now + sampleIntervalMs();
    }
    if (!scd_ok && !lux_ok) { return; }

    Sample s;
    s.utc = (uint32_t)utcNow();
    s.mode = (uint8_t)scd_.mode();
    if (!clockValid()) { s.flags |= Sample::kUnsynced; }
    if (scd_ok) {
        s.flags |= Sample::kScdValid;
        s.co2 = r.co2;
        s.temp_c100 = (int16_t)(r.temp_c * 100.0f);
        s.rh_c100 = (uint16_t)(r.rh * 100.0f);
    }
    if (lux_ok) {
        s.flags |= Sample::kLuxValid;
        s.lux = lux;
    }
    ring_->push(s);
    sd_.queue(s);
    latest_ = s;
    have_latest_ = true;
    ++samples_;
    if (bins_cached_) {
        for (int f = 0; f < kFieldCount; ++f) {
            float v;
            if (sampleValue(s, (PlotField)f, v)) { bins_[f]->add(s.utc, v); }
        }
    }
    ++data_version_;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::calibration(uint32_t now)
{
    if (!calibrating_) { return; }
    uint32_t warm = (scd_.mode() == SamplingMode::Ultra) ? (now - cal_start_ms_) : scd_.runningMs(now);
    if (warm < kScdFrcWarmupMs || shot_pending_) { return; }
    int corr = 0;
    bool ok = scd_.forcedRecalibration(kFrcTargetPpm, corr, now);
    calibrating_ = false;
    waiting_for_scd_ = false;
    next_sample_ms_ = now + sampleIntervalMs();
    char msg[64];
    if (ok) {
        settings_.cal.calibrated(utcNow());
        settings_.save();
        cal_manual_open_ = false;
        snprintf(msg, sizeof(msg), "Calibrated to %u ppm, correction %+d ppm", (unsigned)kFrcTargetPpm, corr);
    } else {
        snprintf(msg, sizeof(msg), "Calibration failed, try again");
    }
    KF_TRACE("%s", msg);
    setMessage(msg, now, kMessageMs);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void App::autoSync(uint32_t now)
{
    if (diff(now, next_autosync_ms_) < 0) { return; }
    next_autosync_ms_ = now + kAutoSyncCheckMs;
    if (!settings_.auto_sync) { return; }
    int64_t age = settings_.last_sync_utc ? utcNow() - settings_.last_sync_utc : INT64_MAX;
    if (age < kAutoSyncAgeS) { return; }
    runSync(false, now);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::runSync(bool manual, uint32_t now)
{
    if (!psm_.usb() && battery_.mv() < kBatterySyncMinMv) {
        if (manual) { setMessage("Battery too low for Wi-Fi", now, kMessageMs); }
        return;
    }
    if (calibrating_) {
        if (manual) { setMessage("Busy calibrating, sync later", now, kMessageMs); }
        return;
    }
    flush(now, true);
    if (manual) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Connecting to %s ...", wifi_.ssid);
        setMessage(msg, now, kSyncManualTimeoutMs + 1000);
        refresh(now);
    }
    bool was_valid = clockValid();
    psm_.onSyncStart();
    in_sync_ = true;
    SyncResult res = runTimeSync(clock_, wifi_.ssid, wifi_.password,
                                 manual ? kSyncManualTimeoutMs : kSyncAutoTimeoutMs,
                                 [this]() { sample(millis()); });
    in_sync_ = false;
    now = millis();
    psm_.onSyncEnd(now);

    char msg[64];
    if (res.code == SyncResult::Ok) {
        rtc_utc_ = res.utc;
        rtc_ms_ = now;
        if (!was_valid) {
            // Data and timers recorded on the unsynced clock move to the real time line.
            ring_->rebaseUnsynced(res.correction_s);
            settings_.cal.rebase(res.correction_s);
            bins_cached_ = false;
        }
        settings_.last_sync_utc = res.utc;
        settings_.save();
        if (display_.busPowered()) { sd_.logSync(res.utc, res.correction_s); }
        snprintf(msg, sizeof(msg), "Sync ok, clock corrected by %+lld s", (long long)res.correction_s);
    } else {
        snprintf(msg, sizeof(msg), "Sync failed: %s", syncResultText(res.code));
    }
    if (manual || res.code == SyncResult::Ok) { setMessage(msg, now, kMessageMs); }
    else { message_[0] = '\0'; }
    markDirty(false, now);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void App::setMessage(const char* msg, uint32_t now, uint32_t duration_ms)
{
    strncpy(message_, msg, sizeof(message_) - 1);
    message_[sizeof(message_) - 1] = '\0';
    message_until_ = now + duration_ms;
    markDirty(false, now);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void App::markDirty(bool full, uint32_t now)
{
    dirty_ = true;
    dirty_full_ = dirty_full_ || full;
    next_refresh_ms_ = now;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int64_t App::utcNow() const
{
    return rtc_utc_ + (int64_t)(millis() - rtc_ms_) / 1000;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool App::clockValid() const
{
    return settings_.last_sync_utc > 0 && !clock_.voltLow();
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
bool App::calBoxVisible(int64_t utc) const
{
    return cal_manual_open_ || calibrating_ || settings_.cal.due(utc, kCalIntervalS);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
int App::graphCols() const
{
    Rect tile = (view_ == View::Plot) ? layout::plotArea(false) : layout::homeTile(0, false);
    return Ui::graphRect(tile).w;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Up to 6 h the bins are rebuilt from the RAM ring on every refresh (cheap). Longer
// ranges stream the SD files once and then get new samples added as they come;
// the window reaches 2 % into the future and is rebuilt when now passes it.
void App::prepareBins(uint32_t now)
{
    (void)now;
    int cols = graphCols();
    int64_t utc = utcNow();
    uint32_t range = rangeSeconds(settings_.range);

    if (range <= kRingSeconds) {
        bins_cached_ = false;
        int64_t t1 = utc + 1;
        for (int f = 0; f < kFieldCount; ++f) { bins_[f]->begin(t1 - range, t1, cols); }
        for (size_t i = 0; i < ring_->size(); ++i) {
            const Sample& s = ring_->at(i);
            for (int f = 0; f < kFieldCount; ++f) {
                float v;
                if (sampleValue(s, (PlotField)f, v)) { bins_[f]->add(s.utc, v); }
            }
        }
        return;
    }

    if (bins_cached_ && bins_range_ == settings_.range && bins_cols_ == cols && utc < bins_t1_) { return; }
    int64_t t1 = utc + range / 48;
    int64_t t0 = t1 - range;
    for (int f = 0; f < kFieldCount; ++f) { bins_[f]->begin(t0, t1, cols); }
    flush(now, true);
    display_.powerForSd();
    int64_t newest = sd_.loadHistory(bins_, t0, t1);
    for (size_t i = 0; i < ring_->size(); ++i) {
        const Sample& s = ring_->at(i);
        if ((int64_t)s.utc <= newest) { continue; }
        for (int f = 0; f < kFieldCount; ++f) {
            float v;
            if (sampleValue(s, (PlotField)f, v)) { bins_[f]->add(s.utc, v); }
        }
    }
    bins_cached_ = true;
    bins_range_ = settings_.range;
    bins_cols_ = cols;
    bins_t1_ = t1;
    ++data_version_;
    KF_TRACE("history rebuilt, range %lu s, newest %lld", (unsigned long)range, (long long)newest);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void App::buildModel(UiModel& m, uint32_t now)
{
    int64_t utc = utcNow();
    m.view = view_;
    m.plot = plot_;
    m.range = settings_.range;
    m.user_mode = settings_.mode;
    m.eff_mode = scd_.mode();
    m.interactive = psm_.interactiveRefresh();
    m.usb = psm_.usb();
    m.utc = utc;
    m.clock_valid = clockValid();
    m.last_sync_utc = settings_.last_sync_utc;
    m.battery_pct = battery_.percent();
    m.sd_problem = sd_.problem();
    bool cal_box = calBoxVisible(utc);
    m.cal_due_mark = cal_box;
    if (message_[0] && diff(now, message_until_) >= 0) { message_[0] = '\0'; }
    m.box = cal_box || message_[0] != '\0';
    m.box_buttons = cal_box;
    m.cal_manual = cal_manual_open_;
    m.calibrating = calibrating_;
    if (calibrating_) {
        uint32_t warm = (scd_.mode() == SamplingMode::Ultra) ? (now - cal_start_ms_) : scd_.runningMs(now);
        m.cal_countdown_s = (warm >= kScdFrcWarmupMs) ? 0 : (int)((kScdFrcWarmupMs - warm + 999) / 1000);
    }
    strncpy(m.message, message_, sizeof(m.message) - 1);
    m.auto_sync = settings_.auto_sync;
    m.temp_offset = settings_.temp_offset;
    m.last_cal_utc = settings_.cal.last_cal_utc;

    bool scd_ok = scd_.present() && scd_missed_ < kScdMissedIntervalsReinit;
    m.sensor_ok[0] = m.sensor_ok[1] = m.sensor_ok[2] = scd_ok;
    m.sensor_ok[3] = light_.present();
    if (view_ != View::Settings) { prepareBins(now); }
    for (int f = 0; f < kFieldCount; ++f) {
        m.bins[f] = bins_[f];
        float v;
        m.has_current[f] = have_latest_ && sampleValue(latest_, (PlotField)f, v);
        if (m.has_current[f]) { m.current[f] = v; }
    }
    uint32_t interval_s = sampleIntervalMs() / 1000;
    int cols = graphCols();
    uint32_t range = rangeSeconds(settings_.range);
    int gap = (int)((2ULL * interval_s * cols + range - 1) / range) + 1;
    m.max_gap_cols = gap < 2 ? 2 : gap;
    m.data_version = data_version_;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::refresh(uint32_t now)
{
    if (!dirty_ && diff(now, next_refresh_ms_) < 0) { return; }
    next_refresh_ms_ = now + psm_.refreshIntervalMs();

    UiModel m;
    buildModel(m, now);
    uint32_t sig = Ui::signature(m);
    bool interactive = psm_.interactiveRefresh();
    if (sig != last_signature_ || dirty_full_) {
        uint32_t t0 = millis();
        display_.beginFrame(dirty_full_ || !interactive);
        ui_.draw(m);
        display_.endFrame();
        last_signature_ = sig;
        KF_TRACE("refresh %s %lu ms", display_.frameIsFull() ? "GC16" : "DU", (unsigned long)(millis() - t0));
    }
    dirty_ = false;
    dirty_full_ = false;
    // SD writes ride on the powered panel.
    flush(now, false);
    display_.rest(!interactive && !in_sync_);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::flush(uint32_t now, bool force)
{
    if (sd_.queued() == 0) { return; }
    bool due = force || diff(now, last_flush_ms_) >= (int32_t)kSdFlushMs || sd_.bufferNearlyFull();
    if (!due) { return; }
    if (!display_.busPowered()) {
        // Panel off (idle): wait for the next idle refresh unless the buffer is about
        // to overflow on a working card.
        if (!(sd_.bufferNearlyFull() && !sd_.problem())) { return; }
        display_.powerForSd();
    }
    sd_.flush();
    last_flush_ms_ = now;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::lowBatteryShutdown()
{
    KF_TRACE("low battery: %d mV, shutting down", battery_.mv());
    psm_.onLowBattery();
    char last[32] = "-";
    if (have_latest_) {
        struct tm lt;
        clock_.local(latest_.utc, lt);
        snprintf(last, sizeof(last), "%02d.%02d. %02d:%02d", lt.tm_mday, lt.tm_mon + 1, lt.tm_hour, lt.tm_min);
    }
    display_.beginFrame(true);
    ui_.drawLowBattery(last);
    display_.endFrame();
    sd_.flush();
    settings_.low_batt_off = true;
    settings_.save();
    display_.rest(true);
    board::powerOff();
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void App::sleep(uint32_t now)
{
    // Stay awake while a finger is down or a double-tap window is open: M5Unified's
    // lightSleep waits for INT release and drops GT911 data, which would lose taps.
    bool touching = false;
    for (int i = 0; i < M5.Touch.getCount(); ++i) {
        if (M5.Touch.getDetail(i).isPressed()) { touching = true; }
    }
    if (touching || taps_.windowOpen()) {
        delay(10);
        return;
    }

    uint32_t wake = now + kMaxSleepMs;
    auto earliest = [&](uint32_t t) { if (diff(t, wake) < 0) { wake = t; } };
    earliest(shot_pending_ ? shot_ready_ms_ : next_sample_ms_);
    earliest(next_refresh_ms_);
    earliest(next_autosync_ms_);
    if (psm_.state() == PowerStateMachine::State::Interactive) { earliest(psm_.interactiveDeadline()); }
    if (message_[0]) { earliest(message_until_); }
    if (calibrating_) {
        // Only a future deadline: once warm, an Ultra FRC waits for the pending shot
        // (already a deadline above), so a past time here would spin.
        uint32_t base = (scd_.mode() == SamplingMode::Ultra) ? cal_start_ms_ : now - scd_.runningMs(now);
        if (diff(base + kScdFrcWarmupMs, now) > 0) { earliest(base + kScdFrcWarmupMs); }
    }

    uint32_t after = millis();
    int32_t ms = diff(wake, after);
    KF_TRACE("wake %lu awake %lu ms, sleep %ld ms, samples %lu, GC16 %lu, DU %lu, state %d, %d mV",
             (unsigned long)wakes_, (unsigned long)(after - now), (long)ms, (unsigned long)samples_,
             (unsigned long)display_.fullCount(), (unsigned long)display_.partialCount(),
             (int)psm_.state(), battery_.mv());
    if (ms <= 0) { return; }
    if (ms < 20) {
        delay(ms);
        return;
    }
    board::sleepUntil((uint32_t)ms);
}

} // namespace kf
