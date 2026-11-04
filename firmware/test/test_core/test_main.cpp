/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Host unit tests for lib/kfcore: pio test -e native

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>
#include "BatteryMonitor.h"
#include "BleProto.h"
#include "CalReminder.h"
#include "ColumnBins.h"
#include "CsvFormat.h"
#include "Layout.h"
#include "PowerStateMachine.h"
#include "RunLog.h"
#include "SampleRing.h"
#include "TapClassifier.h"
#include "TimeUtil.h"
#include "UsbHeuristic.h"
#include "config.h"

using namespace kf;

void setUp() {}
void tearDown() {}

// ---- TimeUtil ----

void test_time_roundtrip()
{
    Civil c{2026, 10, 24, 13, 5, 7};
    int64_t t = utcFromCivil(c);
    TEST_ASSERT_EQUAL_INT64(1792847107LL, t);
    Civil back = civilFromUtc(t);
    TEST_ASSERT_EQUAL(2026, back.year);
    TEST_ASSERT_EQUAL(10, back.month);
    TEST_ASSERT_EQUAL(24, back.day);
    TEST_ASSERT_EQUAL(13, back.hour);
    TEST_ASSERT_EQUAL(5, back.minute);
    TEST_ASSERT_EQUAL(7, back.second);
}

void test_time_leap_day_and_epoch()
{
    Civil c{2028, 2, 29, 0, 0, 0};
    Civil back = civilFromUtc(utcFromCivil(c));
    TEST_ASSERT_EQUAL(2, back.month);
    TEST_ASSERT_EQUAL(29, back.day);
    TEST_ASSERT_EQUAL_INT64(0, utcFromCivil(Civil{1970, 1, 1, 0, 0, 0}));
}

// ---- CSV ----

void test_csv_roundtrip()
{
    Sample s;
    s.utc = (uint32_t)utcFromCivil(Civil{2026, 10, 24, 10, 0, 0});
    s.co2 = 812;
    s.temp_c100 = 2134;
    s.rh_c100 = 8850;
    s.lux = 140.5f;
    s.mode = (uint8_t)SamplingMode::Eco;
    s.flags = Sample::kScdValid | Sample::kLuxValid;
    char row[kCsvRowMax];
    formatRow(s, row, sizeof(row));
    TEST_ASSERT_EQUAL_STRING("2026-10-24T10:00:00Z,812,21.34,88.50,140.5,eco,ntp", row);
    Sample p;
    TEST_ASSERT_TRUE(parseRow(row, p));
    TEST_ASSERT_EQUAL_UINT32(s.utc, p.utc);
    TEST_ASSERT_EQUAL(812, p.co2);
    TEST_ASSERT_EQUAL(2134, p.temp_c100);
    TEST_ASSERT_EQUAL(8850, p.rh_c100);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 140.5f, p.lux);
    TEST_ASSERT_EQUAL(Sample::kScdValid | Sample::kLuxValid, p.flags);
}

void test_csv_missing_sensor_and_unsynced()
{
    Sample s;
    s.utc = 0;
    s.lux = 3.0f;
    s.mode = (uint8_t)SamplingMode::Ultra;
    s.flags = Sample::kLuxValid | Sample::kUnsynced;
    char row[kCsvRowMax];
    formatRow(s, row, sizeof(row));
    TEST_ASSERT_EQUAL_STRING("1970-01-01T00:00:00Z,,,,3.0,ultra,unsynced", row);
    Sample p;
    TEST_ASSERT_TRUE(parseRow(row, p));
    TEST_ASSERT_FALSE(p.flags & Sample::kScdValid);
    TEST_ASSERT_TRUE(p.flags & Sample::kUnsynced);
    TEST_ASSERT_EQUAL((uint8_t)SamplingMode::Ultra, p.mode);
    TEST_ASSERT_FALSE(parseRow(kCsvHeader, p));
}

// ---- RunLog ----

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void test_run_paths()
{
    char dir[kRunDirMax];
    Civil d{2026, 11, 3, 0, 0, 0};
    runDirName(4, &d, dir, sizeof(dir));
    TEST_ASSERT_EQUAL_STRING("m004_2026-11-03", dir);
    runDirName(1, nullptr, dir, sizeof(dir));
    TEST_ASSERT_EQUAL_STRING("m001_unsynced", dir);
    runDirName(1234, &d, dir, sizeof(dir));
    TEST_ASSERT_EQUAL_STRING("m1234_2026-11-03", dir);

    char path[kLogPathMax];
    runDayLogPath("m004_2026-11-03", utcFromCivil(Civil{2026, 11, 4, 23, 59, 59}), path, sizeof(path));
    TEST_ASSERT_EQUAL_STRING("/log/m004_2026-11-03/2026-11-04.csv", path);
    runFilePath("m004_2026-11-03", kUnsyncedName, path, sizeof(path));
    TEST_ASSERT_EQUAL_STRING("/log/m004_2026-11-03/unsynced.csv", path);
    runFilePath("m004_2026-11-03", kRunInfoName, path, sizeof(path));
    TEST_ASSERT_EQUAL_STRING("/log/m004_2026-11-03/run.txt", path);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void test_run_numbering()
{
    TEST_ASSERT_EQUAL(4, parseRunNumber("m004_2026-11-03"));
    TEST_ASSERT_EQUAL(1, parseRunNumber("/log/m001_unsynced"));
    TEST_ASSERT_EQUAL(1234, parseRunNumber("m1234_2026-11-03"));
    TEST_ASSERT_EQUAL(-1, parseRunNumber("2026-11-03.csv"));
    TEST_ASSERT_EQUAL(-1, parseRunNumber("m04_2026-11-03"));    // fewer than 3 digits
    TEST_ASSERT_EQUAL(-1, parseRunNumber("m004"));              // no underscore
    TEST_ASSERT_EQUAL(-1, parseRunNumber("error.log"));
    TEST_ASSERT_EQUAL(1, nextRunNumber(0, 0));                  // first run
    TEST_ASSERT_EQUAL(5, nextRunNumber(4, 4));
    TEST_ASSERT_EQUAL(8, nextRunNumber(4, 7));                  // swapped card with more runs
    TEST_ASSERT_EQUAL(5, nextRunNumber(4, 0));                  // empty new card
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void test_run_day_index()
{
    // Start 3 Nov 23:30 local (CET, +1 h) = 22:30 UTC.
    int64_t start = utcFromCivil(Civil{2026, 11, 3, 22, 30, 0});
    TEST_ASSERT_EQUAL(1, runDayIndex(start, 3600, start + 600, 3600));
    TEST_ASSERT_EQUAL(2, runDayIndex(start, 3600, start + 3600, 3600));       // past local midnight
    TEST_ASSERT_EQUAL(12, runDayIndex(start, 3600, start + 11 * 86400, 3600));
    TEST_ASSERT_EQUAL(1, runDayIndex(start, 3600, start - 7200, 3600));       // clock went back
    // Start in summer (CEST, +2 h) at 00:30 local, now in winter at 00:30 local.
    int64_t s2 = utcFromCivil(Civil{2026, 10, 24, 22, 30, 0});                // 25 Oct 00:30 CEST
    int64_t n2 = utcFromCivil(Civil{2026, 10, 26, 23, 30, 0});                // 27 Oct 00:30 CET
    TEST_ASSERT_EQUAL(3, runDayIndex(s2, 7200, n2, 3600));
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void test_run_info_text()
{
    RunInfo r;
    r.run = 4;
    snprintf(r.dir, sizeof(r.dir), "m004_2026-11-03");
    r.start_utc = utcFromCivil(Civil{2026, 11, 3, 9, 12, 0});
    r.last_cal_utc = 0;
    r.temp_offset = 4.5f;
    char buf[320];
    formatRunInfo(r, "2026.11.03", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("run=4\nstart_utc=2026-11-03T09:12:00Z\nclock=synced\nfirmware=2026.11.03\n"
                             "last_cal_utc=never\ntemp_offset_c=4.5\n", buf);
    snprintf(r.dir, sizeof(r.dir), "m001_unsynced");
    r.last_cal_utc = utcFromCivil(Civil{2026, 10, 28, 8, 0, 0});
    formatRunInfo(r, "x", buf, sizeof(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "clock=unsynced\n"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "last_cal_utc=2026-10-28T08:00:00Z\n"));
}

// ---- SampleRing ----

void test_ring_wrap_and_rebase()
{
    Sample store[3];
    SampleRing ring(store, 3);
    for (uint32_t i = 1; i <= 5; ++i) {
        Sample s;
        s.utc = i;
        s.flags = (i == 5) ? 0 : Sample::kUnsynced;
        ring.push(s);
    }
    TEST_ASSERT_EQUAL(3, (int)ring.size());
    TEST_ASSERT_EQUAL_UINT32(3, ring.at(0).utc);
    TEST_ASSERT_EQUAL_UINT32(5, ring.newest().utc);
    ring.rebaseUnsynced(100);
    TEST_ASSERT_EQUAL_UINT32(103, ring.at(0).utc);
    TEST_ASSERT_EQUAL_UINT32(104, ring.at(1).utc);
    TEST_ASSERT_EQUAL_UINT32(5, ring.at(2).utc);   // was synced: unchanged
    TEST_ASSERT_FALSE(ring.at(0).flags & Sample::kUnsynced);
}

// ---- ColumnBins ----

void test_bins_min_max_per_column()
{
    static ColumnBins b;
    b.begin(0, 100, 10);
    b.add(0, 5.0f);
    b.add(9, 7.0f);
    b.add(10, 1.0f);
    b.add(99, 3.0f);
    b.add(100, 99.0f);   // outside the window
    b.add(-1, 99.0f);
    TEST_ASSERT_TRUE(b.has(0));
    TEST_ASSERT_EQUAL_FLOAT(5.0f, b.min(0));
    TEST_ASSERT_EQUAL_FLOAT(7.0f, b.max(0));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, b.min(1));
    TEST_ASSERT_FALSE(b.has(5));
    TEST_ASSERT_TRUE(b.has(9));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, b.overallMin());
    TEST_ASSERT_EQUAL_FLOAT(7.0f, b.overallMax());
    TEST_ASSERT_EQUAL_FLOAT(3.0f, b.last());
}

// ---- TapClassifier ----

void test_tap_actionable_is_deferred()
{
    TapClassifier t(400);
    TEST_ASSERT_EQUAL(TapEvent::None, t.onTap(1000, 10, 20, true).kind);
    TEST_ASSERT_EQUAL(TapEvent::None, t.poll(1300).kind);
    TapEvent ev = t.poll(1401);
    TEST_ASSERT_EQUAL(TapEvent::Tap, ev.kind);
    TEST_ASSERT_EQUAL(10, ev.x);
    TEST_ASSERT_FALSE(t.windowOpen());
}

void test_tap_double_suppresses_single()
{
    TapClassifier t(400);
    t.onTap(1000, 10, 20, true);
    TEST_ASSERT_EQUAL(TapEvent::DoubleTap, t.onTap(1350, 12, 22, true).kind);
    TEST_ASSERT_EQUAL(TapEvent::None, t.poll(2000).kind);
}

void test_tap_non_actionable_still_starts_double()
{
    TapClassifier t(400);
    t.onTap(1000, 10, 20, false);
    TEST_ASSERT_EQUAL(TapEvent::None, t.poll(1500).kind);   // nothing to report
    t.onTap(2000, 10, 20, false);
    TEST_ASSERT_EQUAL(TapEvent::DoubleTap, t.onTap(2200, 10, 20, false).kind);
}

void test_tap_slow_second_tap_is_new_tap()
{
    TapClassifier t(400);
    t.onTap(1000, 1, 1, true);
    TEST_ASSERT_EQUAL(TapEvent::None, t.onTap(1500, 2, 2, true).kind);  // window passed
    TEST_ASSERT_EQUAL(TapEvent::Tap, t.poll(1901).kind);
}

// ---- UsbHeuristic ----

UsbHeuristic::Params usbParams()
{
    return UsbHeuristic::Params{4150, 4100, 30, 20, 10UL * 60000, 60000};
}

void test_usb_flat_high_needs_full_window()
{
    UsbHeuristic h(usbParams());
    uint32_t t = 0;
    for (int i = 0; i < 10; ++i, t += 60000) { TEST_ASSERT_FALSE(h.feed(t, 4190)); }
    TEST_ASSERT_TRUE(h.feed(t, 4190));   // 10 min of points
}

void test_usb_charging_rise_below_leave_level()
{
    UsbHeuristic h(usbParams());
    uint32_t t = 0;
    int mv = 3800;
    bool usb = false;
    for (int i = 0; i <= 10; ++i, t += 60000, mv += 4) { usb = h.feed(t, mv); }
    TEST_ASSERT_TRUE(usb);
    // Still charging at 3.85 V: "below 4.10 V" must not end USB (it is a crossing rule).
    for (int i = 0; i < 30; ++i, t += 60000, mv += 2) { TEST_ASSERT_TRUE(h.feed(t, mv)); }
}

void test_usb_unplugged_full_cell_does_not_reenter()
{
    UsbHeuristic h(usbParams());
    uint32_t t = 0;
    for (int i = 0; i <= 10; ++i, t += 60000) { h.feed(t, 4200); }
    TEST_ASSERT_TRUE(h.usb());
    h.feed(t, 4175); t += 60000;
    TEST_ASSERT_FALSE(h.feed(t, 4174)); t += 60000;
    // Full cell on battery sits flat above 4.15 V for a long time: stays battery.
    int mv = 4174;
    for (int i = 0; i < 120; ++i, t += 60000) {
        TEST_ASSERT_FALSE(h.feed(t, mv));
        if (i % 20 == 0) { mv -= 1; }
    }
}

void test_usb_leave_on_crossing()
{
    UsbHeuristic h(usbParams());
    uint32_t t = 0;
    for (int i = 0; i <= 10; ++i, t += 60000) { h.feed(t, 4160); }
    TEST_ASSERT_TRUE(h.usb());
    // Fall of 1 mV/min: never 20 mV within the 10 min window, so only the crossing ends USB.
    int mv = 4160;
    while (mv > 4100) {
        --mv;
        t += 60000;
        TEST_ASSERT_TRUE(h.feed(t, mv));
    }
    t += 60000;
    TEST_ASSERT_FALSE(h.feed(t, 4099));
}

void test_usb_unplug_drop()
{
    UsbHeuristic h(usbParams());
    uint32_t t = 0;
    for (int i = 0; i <= 10; ++i, t += 60000) { h.feed(t, 4200); }
    TEST_ASSERT_TRUE(h.usb());
    TEST_ASSERT_TRUE(h.feed(t, 4170)); t += 60000;   // first drop hit
    TEST_ASSERT_FALSE(h.feed(t, 4168));              // second: battery
}

void test_usb_ignores_points_faster_than_step()
{
    UsbHeuristic h(usbParams());
    for (uint32_t t = 0; t < 11UL * 60000; t += 5000) { h.feed(t, 4190); }
    TEST_ASSERT_TRUE(h.usb());
}

// ---- PowerStateMachine ----

PowerStateMachine::Params psmParams()
{
    return PowerStateMachine::Params{300000, 300000, 5000, 30000};
}

void test_psm_boot_interactive_then_idle()
{
    PowerStateMachine p(psmParams());
    p.begin(0, SamplingMode::Eco, false);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Interactive, (int)p.state());
    TEST_ASSERT_EQUAL_UINT32(30000, p.refreshIntervalMs());
    p.update(299999);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Interactive, (int)p.state());
    p.update(300000);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Idle, (int)p.state());
    TEST_ASSERT_EQUAL_UINT32(300000, p.refreshIntervalMs());
    TEST_ASSERT_FALSE(p.interactiveRefresh());
}

void test_psm_touch_restarts_timer()
{
    PowerStateMachine p(psmParams());
    p.begin(0, SamplingMode::Fast, false);
    p.onTouch(200000);
    p.update(400000);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Interactive, (int)p.state());
    TEST_ASSERT_EQUAL_UINT32(5000, p.refreshIntervalMs());
    p.update(500000);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Idle, (int)p.state());
    p.onTouch(600000);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Interactive, (int)p.state());
}

void test_psm_usb_forces_fast_without_timeout()
{
    PowerStateMachine p(psmParams());
    p.begin(0, SamplingMode::Ultra, false);
    TEST_ASSERT_EQUAL((int)SamplingMode::Ultra, (int)p.samplingMode());
    p.onUsb(1000, true);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::UsbPerf, (int)p.state());
    TEST_ASSERT_EQUAL((int)SamplingMode::Fast, (int)p.samplingMode());
    p.update(10000000);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::UsbPerf, (int)p.state());
    TEST_ASSERT_EQUAL_UINT32(5000, p.refreshIntervalMs());
    p.onUsb(10000001, false);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Idle, (int)p.state());
    TEST_ASSERT_EQUAL((int)SamplingMode::Ultra, (int)p.samplingMode());
}

void test_psm_usb_change_during_sync_applied_after()
{
    PowerStateMachine p(psmParams());
    p.begin(0, SamplingMode::Eco, false);
    p.onSyncStart();
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Syncing, (int)p.state());
    p.onUsb(100, true);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::Syncing, (int)p.state());
    TEST_ASSERT_EQUAL((int)SamplingMode::Eco, (int)p.samplingMode());
    p.onSyncEnd(200);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::UsbPerf, (int)p.state());
}

void test_psm_low_battery_terminal()
{
    PowerStateMachine p(psmParams());
    p.begin(0, SamplingMode::Eco, false);
    p.onLowBattery();
    p.onUsb(1, true);
    p.onTouch(2);
    TEST_ASSERT_EQUAL((int)PowerStateMachine::State::LowBattOff, (int)p.state());
}

// ---- Layout ----

/* Edited by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void test_layout_hits()
{
    UiShape ui;
    ui.view = View::Home;
    Hit h = layout::hitTest(ui, 100, 100);
    TEST_ASSERT_EQUAL((int)Target::PlotTile, (int)h.target);
    TEST_ASSERT_EQUAL(0, h.index);
    h = layout::hitTest(ui, 600, 400);
    TEST_ASSERT_EQUAL(3, h.index);
    Rect s = layout::syncButton();
    TEST_ASSERT_EQUAL((int)Target::Sync, (int)layout::hitTest(ui, s.x + 5, s.y + 5).target);

    ui.view = View::Plot;
    h = layout::hitTest(ui, 100, 100);
    TEST_ASSERT_EQUAL((int)Target::PlotArea, (int)h.target);
    TEST_ASSERT_FALSE(layout::actionable(h));

    ui.view = View::Home;
    ui.cal_box = true;
    ui.box_buttons = true;
    ui.cal_ready = true;
    Rect c = layout::calConfirmButton();
    TEST_ASSERT_EQUAL((int)Target::CalConfirm, (int)layout::hitTest(ui, c.x + 5, c.y + 5).target);
    Rect p = layout::calSecondButton();
    TEST_ASSERT_EQUAL((int)Target::CalPostpone, (int)layout::hitTest(ui, p.x + 5, p.y + 5).target);
    ui.cal_ready = false;   // calibrating: second button cancels
    TEST_ASSERT_EQUAL((int)Target::CalCancel, (int)layout::hitTest(ui, p.x + 5, p.y + 5).target);
    ui.box_buttons = false; // message strip only
    TEST_ASSERT_EQUAL((int)Target::None, (int)layout::hitTest(ui, c.x + 5, c.y + 5).target);
}

void test_layout_settings_targets_do_not_overlap_sidebar()
{
    for (int i = 0; i < 4; ++i) {
        Rect r = layout::rangeButton(i);
        TEST_ASSERT_TRUE(r.x + r.w <= layout::kSidebarX);
    }
    Rect c = layout::calibrateNowButton();
    TEST_ASSERT_TRUE(c.x + c.w <= layout::kSidebarX);
    Rect n = layout::newRunButton();
    TEST_ASSERT_TRUE(n.x + n.w <= layout::kSidebarX);
    TEST_ASSERT_TRUE(n.y >= c.y + c.h);
    TEST_ASSERT_TRUE(n.y + n.h <= 540);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void test_layout_new_run_and_question()
{
    UiShape ui;
    ui.view = View::Settings;
    Rect n = layout::newRunButton();
    TEST_ASSERT_EQUAL((int)Target::NewRun, (int)layout::hitTest(ui, n.x + 5, n.y + 5).target);
    Rect c = layout::calibrateNowButton();
    TEST_ASSERT_EQUAL((int)Target::CalibrateNow, (int)layout::hitTest(ui, c.x + 5, c.y + c.h - 5).target);

    ui.view = View::Home;
    ui.cal_box = true;
    ui.box_buttons = true;
    ui.cal_ready = true;
    ui.ask = true;   // question wins over a due calibration box
    Rect y = layout::calConfirmButton();
    TEST_ASSERT_EQUAL((int)Target::AskYes, (int)layout::hitTest(ui, y.x + 5, y.y + 5).target);
    Rect s = layout::calSecondButton();
    TEST_ASSERT_EQUAL((int)Target::AskNo, (int)layout::hitTest(ui, s.x + 5, s.y + 5).target);
}

// ---- CalReminder and BatteryMonitor ----

void test_cal_reminder()
{
    CalReminder c;
    c.calibrated(1000);
    TEST_ASSERT_FALSE(c.due(1000 + kCalIntervalS - 1, kCalIntervalS));
    TEST_ASSERT_TRUE(c.due(1000 + kCalIntervalS, kCalIntervalS));
    c.postpone(1000 + kCalIntervalS, kCalSnoozeS);
    TEST_ASSERT_FALSE(c.due(1000 + kCalIntervalS + 3600, kCalIntervalS));
    TEST_ASSERT_TRUE(c.due(1000 + kCalIntervalS + kCalSnoozeS, kCalIntervalS));
    c.rebase(500);
    TEST_ASSERT_EQUAL_INT64(1500, c.last_cal_utc);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void test_cal_never_and_base()
{
    // First boot: the reminder starts at the base, but nothing was calibrated.
    CalReminder c;
    TEST_ASSERT_FALSE(c.everCalibrated());
    c.base_utc = 1000;
    TEST_ASSERT_FALSE(c.everCalibrated());
    TEST_ASSERT_FALSE(c.due(1000 + kCalIntervalS - 1, kCalIntervalS));
    TEST_ASSERT_TRUE(c.due(1000 + kCalIntervalS, kCalIntervalS));
    // A real calibration after the base moves the interval.
    c.calibrated(5000);
    TEST_ASSERT_TRUE(c.everCalibrated());
    TEST_ASSERT_FALSE(c.due(1000 + kCalIntervalS, kCalIntervalS));
    TEST_ASSERT_TRUE(c.due(5000 + kCalIntervalS, kCalIntervalS));
    // Rebase leaves "never" alone.
    CalReminder n;
    n.base_utc = 100;
    n.rebase(1000);
    TEST_ASSERT_EQUAL_INT64(0, n.last_cal_utc);
    TEST_ASSERT_EQUAL_INT64(1100, n.base_utc);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void test_cal_legacy_migration()
{
    // Old firmware: first boot wrote the boot time as last_cal_utc.
    CalReminder c;
    c.last_cal_utc = 2000;
    c.snooze_until_utc = 0;
    c.migrateLegacy();
    TEST_ASSERT_FALSE(c.everCalibrated());
    TEST_ASSERT_EQUAL_INT64(2000, c.base_utc);
    TEST_ASSERT_FALSE(c.due(2000 + kCalIntervalS - 1, kCalIntervalS));   // reminder timing kept
    TEST_ASSERT_TRUE(c.due(2000 + kCalIntervalS, kCalIntervalS));
    // A fresh device: nothing stored, nothing set.
    CalReminder f;
    f.migrateLegacy();
    TEST_ASSERT_EQUAL_INT64(0, f.intervalStart());
}

void test_battery_low_needs_consecutive_readings()
{
    BatteryMonitor b(3550, 3, 3550, 4150);
    TEST_ASSERT_FALSE(b.feed(3500, false));
    TEST_ASSERT_FALSE(b.feed(3500, false));
    TEST_ASSERT_FALSE(b.feed(3600, false));   // recovery resets the count
    TEST_ASSERT_FALSE(b.feed(3500, false));
    TEST_ASSERT_FALSE(b.feed(3500, false));
    TEST_ASSERT_TRUE(b.feed(3500, false));
    TEST_ASSERT_FALSE(b.feed(3000, true));    // never on USB
    b.feed(3850, false);
    TEST_ASSERT_EQUAL(50, b.percent());
}

// ---- Layout: Wi-Fi sync off ----

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void test_layout_wifi_targets_follow_flag()
{
    UiShape ui;
    ui.view = View::Settings;
    Rect a = layout::autoSyncButton();
    TEST_ASSERT_EQUAL((int)(kWifiSync ? Target::AutoSync : Target::None),
                      (int)layout::hitTest(ui, a.x + 5, a.y + 5).target);
    // Bluetooth box while connected: its Stop button is the second box button.
    ui.view = View::Home;
    ui.cal_box = true;
    ui.box_buttons = true;
    ui.ask = true;
    Rect s = layout::calSecondButton();
    TEST_ASSERT_EQUAL((int)Target::AskNo, (int)layout::hitTest(ui, s.x + 5, s.y + 5).target);
}

// ---- BLE protocol ----

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void test_ble_crc32()
{
    const uint8_t* v = (const uint8_t*)"123456789";
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, bleproto::crc32Update(0, v, 9));
    uint32_t c = bleproto::crc32Update(0, v, 4);
    c = bleproto::crc32Update(c, v + 4, 0);
    c = bleproto::crc32Update(c, v + 4, 5);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, c);
    TEST_ASSERT_EQUAL_HEX32(0u, bleproto::crc32Update(0, v, 0));
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void test_ble_paths()
{
    using bleproto::validPath;
    TEST_ASSERT_TRUE(validPath("error.log"));
    TEST_ASSERT_TRUE(validPath("sync.csv"));
    TEST_ASSERT_TRUE(validPath("m004_2026-11-03/2026-11-03.csv"));
    TEST_ASSERT_TRUE(validPath("m004_2026-11-03/run.txt"));
    TEST_ASSERT_TRUE(validPath("m1000_unsynced/unsynced.csv"));
    TEST_ASSERT_FALSE(validPath(""));
    TEST_ASSERT_FALSE(validPath("2026-10-24.csv"));           // old flat file
    TEST_ASSERT_FALSE(validPath("config.json"));
    TEST_ASSERT_FALSE(validPath("/error.log"));
    TEST_ASSERT_FALSE(validPath("m004_x/"));
    TEST_ASSERT_FALSE(validPath("m004_x/../config.json"));
    TEST_ASSERT_FALSE(validPath("m004_x/a/b.csv"));
    TEST_ASSERT_FALSE(validPath("m004_x/.hidden"));
    TEST_ASSERT_FALSE(validPath("m04_x/a.csv"));               // not a run folder
    TEST_ASSERT_FALSE(validPath("bench/a.csv"));
    TEST_ASSERT_FALSE(validPath("m004_x\\a.csv"));
    TEST_ASSERT_FALSE(validPath("m004_x/a b.csv"));
    TEST_ASSERT_FALSE(validPath("m004_2026-11-03/aaaaaaaaaaaaaaaaaaaaaaaaa.csv"));   // too long
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void test_ble_parse_requests()
{
    using namespace bleproto;
    Request r;
    Status st;
    uint8_t list[] = {kOpList};
    TEST_ASSERT_TRUE(parseRequest(list, 1, r, st));
    TEST_ASSERT_EQUAL(kOpList, r.op);
    TEST_ASSERT_FALSE(parseRequest(list, 0, r, st));
    uint8_t list2[] = {kOpList, 0};
    TEST_ASSERT_FALSE(parseRequest(list2, 2, r, st));
    TEST_ASSERT_EQUAL(kBadRequest, st);

    uint8_t rd[64] = {kOpRead, 0x10, 0x27, 0x00, 0x00};   // offset 10000
    const char* p = "m004_2026-11-03/2026-11-03.csv";
    memcpy(rd + 5, p, strlen(p));
    TEST_ASSERT_TRUE(parseRequest(rd, 5 + strlen(p), r, st));
    TEST_ASSERT_EQUAL(kOpRead, r.op);
    TEST_ASSERT_EQUAL_UINT32(10000u, r.offset);
    TEST_ASSERT_EQUAL_STRING(p, r.path);
    TEST_ASSERT_FALSE(parseRequest(rd, 5, r, st));          // no path
    memcpy(rd + 5, "config.json", 11);
    TEST_ASSERT_FALSE(parseRequest(rd, 16, r, st));
    TEST_ASSERT_EQUAL(kBadPath, st);
    uint8_t nul[] = {kOpRead, 0, 0, 0, 0, 'e', 0, 'x'};       // embedded NUL
    TEST_ASSERT_FALSE(parseRequest(nul, sizeof(nul), r, st));

    uint8_t ack[] = {kOpAck, 0x00, 0x20, 0x00, 0x00};
    TEST_ASSERT_TRUE(parseRequest(ack, 5, r, st));
    TEST_ASSERT_EQUAL_UINT32(8192u, r.offset);
    TEST_ASSERT_FALSE(parseRequest(ack, 4, r, st));
    uint8_t bad[] = {0x55};
    TEST_ASSERT_FALSE(parseRequest(bad, 1, r, st));
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void test_ble_chunks_and_window()
{
    using namespace bleproto;
    TEST_ASSERT_EQUAL(20u, maxPacket(23));
    TEST_ASSERT_EQUAL(15u, dataPayload(23));
    TEST_ASSERT_EQUAL(244u, maxPacket(247));
    TEST_ASSERT_EQUAL(514u, maxPacket(517));
    TEST_ASSERT_EQUAL(514u, maxPacket(1000));

    uint8_t data[3] = {'a', 'b', 'c'};
    uint8_t out[16];
    TEST_ASSERT_EQUAL(8u, encodeData(0x01020304u, data, 3, out));
    TEST_ASSERT_EQUAL_HEX8(kOpData, out[0]);
    TEST_ASSERT_EQUAL_UINT32(0x01020304u, getU32(out + 1));
    TEST_ASSERT_EQUAL('c', out[7]);
    TEST_ASSERT_EQUAL(kReadEndSize, encodeReadEnd(kOk, 300, 0xCBF43926u, out));
    TEST_ASSERT_EQUAL_UINT32(300u, getU32(out + 2));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, getU32(out + 6));

    // 20000-byte file from offset 1000, 8 KB window.
    ReadWindow w;
    w.begin(1000, 20000);
    TEST_ASSERT_EQUAL_UINT32(8192u, w.allowed());
    w.sent(5000);
    TEST_ASSERT_EQUAL_UINT32(3192u, w.allowed());
    w.sent(3192);
    TEST_ASSERT_EQUAL_UINT32(0u, w.allowed());   // window full until an ACK
    w.ack(500);                                    // old ACK ignored
    TEST_ASSERT_EQUAL_UINT32(0u, w.allowed());
    w.ack(5096);
    TEST_ASSERT_EQUAL_UINT32(4096u, w.allowed());
    w.ack(999999);                                 // clamped to what was sent
    TEST_ASSERT_EQUAL_UINT32(8192u, w.allowed());
    w.sent(8192);
    w.ack(w.next());
    TEST_ASSERT_EQUAL_UINT32(20000u - 17384u, w.allowed());   // only up to the end
    w.sent(w.allowed());
    TEST_ASSERT_TRUE(w.done());
    TEST_ASSERT_EQUAL_UINT32(0u, w.allowed());

    // Resume beyond the end (file shrank): nothing to send, done.
    w.begin(30000, 20000);
    TEST_ASSERT_TRUE(w.done());
    TEST_ASSERT_EQUAL_UINT32(20000u, w.next());
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void test_ble_list_packer()
{
    using namespace bleproto;
    ListPacker lp;
    lp.begin(40);   // small packets to force a split
    TEST_ASSERT_FALSE(lp.hasLines());
    TEST_ASSERT_TRUE(lp.add("error.log", 1234));      // 15 bytes
    TEST_ASSERT_TRUE(lp.add("sync.csv", 56));         // 12 bytes, total 3 + 27 = 30
    TEST_ASSERT_FALSE(lp.add("m004_2026-11-03/run.txt", 120));   // 28 bytes, does not fit
    TEST_ASSERT_EQUAL(30u, lp.size());
    TEST_ASSERT_EQUAL_HEX8(kOpListLines, lp.packet()[0]);
    TEST_ASSERT_EQUAL(0, lp.packet()[1] | (lp.packet()[2] << 8));
    TEST_ASSERT_EQUAL_MEMORY("error.log,1234\nsync.csv,56\n", lp.packet() + 3, 27);
    lp.next();
    TEST_ASSERT_TRUE(lp.add("m004_2026-11-03/run.txt", 120));
    TEST_ASSERT_EQUAL(1, lp.packet()[1]);
    TEST_ASSERT_EQUAL(3u, lp.count());
    uint8_t end[kListEndSize];
    lp.encodeEnd(end);
    const char* all = "error.log,1234\nsync.csv,56\nm004_2026-11-03/run.txt,120\n";
    TEST_ASSERT_EQUAL_HEX8(kOpListEnd, end[0]);
    TEST_ASSERT_EQUAL(3, end[1]);
    TEST_ASSERT_EQUAL_HEX32(crc32Update(0, (const uint8_t*)all, strlen(all)), getU32(end + 3));
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void test_ble_live_time_status()
{
    using namespace bleproto;
    Sample s;
    s.utc = 1792847107u;
    s.co2 = 812;
    s.temp_c100 = -125;
    s.rh_c100 = 8850;
    s.lux = 140.5f;
    s.mode = 1;
    s.flags = Sample::kScdValid | Sample::kLuxValid;
    uint8_t out[kLiveSize];
    TEST_ASSERT_EQUAL(16u, encodeLive(s, out));
    TEST_ASSERT_EQUAL_UINT32(1792847107u, getU32(out));
    TEST_ASSERT_EQUAL(812, out[4] | (out[5] << 8));
    TEST_ASSERT_EQUAL(-125, (int16_t)(out[6] | (out[7] << 8)));
    float lux;
    uint32_t raw = getU32(out + 10);
    memcpy(&lux, &raw, 4);
    TEST_ASSERT_EQUAL_FLOAT(140.5f, lux);
    TEST_ASSERT_EQUAL(6, out[15]);

    uint8_t t[8] = {0xB8, 0xD3, 0x03, 0x6E, 0xA1, 0x01, 0x00, 0x00};   // 1792847107000 ms
    int64_t ms = 0;
    TEST_ASSERT_TRUE(decodeTimeMs(t, 8, ms));
    TEST_ASSERT_EQUAL_INT64(1792847107000LL, ms);
    TEST_ASSERT_FALSE(decodeTimeMs(t, 7, ms));

    StatusInfo si;
    si.fw = "2026.11.03";
    si.utc_ms = 1792847107000LL;
    si.run_dir = "m004_2026-11-03";
    si.sd = "ok";
    char buf[320];
    size_t n = formatStatus(si, buf, sizeof(buf));
    TEST_ASSERT_TRUE(n > 100 && n < 300);
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"utc_ms\":1792847107000,"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "\"run_dir\":\"m004_2026-11-03\""));
    TEST_ASSERT_EQUAL('}', buf[n - 1]);
}

int runAll()
{
    UNITY_BEGIN();
    RUN_TEST(test_time_roundtrip);
    RUN_TEST(test_time_leap_day_and_epoch);
    RUN_TEST(test_csv_roundtrip);
    RUN_TEST(test_csv_missing_sensor_and_unsynced);
    RUN_TEST(test_run_paths);
    RUN_TEST(test_run_numbering);
    RUN_TEST(test_run_day_index);
    RUN_TEST(test_run_info_text);
    RUN_TEST(test_ring_wrap_and_rebase);
    RUN_TEST(test_bins_min_max_per_column);
    RUN_TEST(test_tap_actionable_is_deferred);
    RUN_TEST(test_tap_double_suppresses_single);
    RUN_TEST(test_tap_non_actionable_still_starts_double);
    RUN_TEST(test_tap_slow_second_tap_is_new_tap);
    RUN_TEST(test_usb_flat_high_needs_full_window);
    RUN_TEST(test_usb_charging_rise_below_leave_level);
    RUN_TEST(test_usb_unplugged_full_cell_does_not_reenter);
    RUN_TEST(test_usb_leave_on_crossing);
    RUN_TEST(test_usb_unplug_drop);
    RUN_TEST(test_usb_ignores_points_faster_than_step);
    RUN_TEST(test_psm_boot_interactive_then_idle);
    RUN_TEST(test_psm_touch_restarts_timer);
    RUN_TEST(test_psm_usb_forces_fast_without_timeout);
    RUN_TEST(test_psm_usb_change_during_sync_applied_after);
    RUN_TEST(test_psm_low_battery_terminal);
    RUN_TEST(test_layout_hits);
    RUN_TEST(test_layout_settings_targets_do_not_overlap_sidebar);
    RUN_TEST(test_layout_new_run_and_question);
    RUN_TEST(test_cal_reminder);
    RUN_TEST(test_cal_never_and_base);
    RUN_TEST(test_cal_legacy_migration);
    RUN_TEST(test_battery_low_needs_consecutive_readings);
    RUN_TEST(test_layout_wifi_targets_follow_flag);
    RUN_TEST(test_ble_crc32);
    RUN_TEST(test_ble_paths);
    RUN_TEST(test_ble_parse_requests);
    RUN_TEST(test_ble_chunks_and_window);
    RUN_TEST(test_ble_list_packer);
    RUN_TEST(test_ble_live_time_status);
    return UNITY_END();
}

#ifdef ARDUINO
#include <Arduino.h>
// On the device: pio test -e m5paper
void setup()
{
    delay(2000);
    runAll();
}
void loop() {}
#else
int main(int, char**)
{
    return runAll();
}
#endif
