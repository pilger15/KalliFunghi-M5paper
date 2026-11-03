/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Unit tests for lib/kfcore: pio test -e m5paper (on the device) or -e native

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unity.h>
#include "BatteryMonitor.h"
#include "CalReminder.h"
#include "ColumnBins.h"
#include "CsvFormat.h"
#include "Keyboard.h"
#include "Layout.h"
#include "PowerStateMachine.h"
#include "RunLog.h"
#include "SampleRing.h"
#include "TapClassifier.h"
#include "TimeUtil.h"
#include "UsbHeuristic.h"
#include "WifiLayout.h"
#include "WifiList.h"
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

// ---- Keyboard ----
// Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None

void test_kb_keys_tile_area_and_are_big_enough()
{
    Rect a = kb::keyArea();
    TEST_ASSERT_EQUAL(540, a.y + a.h);
    long area = 0;
    for (int i = 0; i < kb::kKeyCount; ++i) {
        kb::Key k = kb::key(kb::Layer::Lower, i);
        TEST_ASSERT_TRUE(k.kind != kb::KeyKind::None);
        TEST_ASSERT_TRUE(k.r.w >= 72 && k.r.h >= 64);
        TEST_ASSERT_TRUE(k.r.x >= 0 && k.r.x + k.r.w <= 960 && k.r.y >= a.y && k.r.y + k.r.h <= 540);
        area += (long)k.r.w * k.r.h;
        for (int j = 0; j < i; ++j) {
            Rect o = kb::key(kb::Layer::Lower, j).r;
            bool overlap = k.r.x < o.x + o.w && o.x < k.r.x + k.r.w && k.r.y < o.y + o.h && o.y < k.r.y + k.r.h;
            TEST_ASSERT_FALSE(overlap);
        }
    }
    TEST_ASSERT_EQUAL((long)a.w * a.h, area);   // no gaps, no overlaps
    // Hit test finds the key at its centre; the Show/Hide button sits in the field.
    kb::Key q = kb::key(kb::Layer::Lower, 0);
    TEST_ASSERT_EQUAL('q', kb::hitTest(kb::Layer::Lower, q.r.x + q.r.w / 2, q.r.y + q.r.h / 2).ch);
    Rect sh = kb::showHideButton();
    TEST_ASSERT_EQUAL((int)kb::KeyKind::ShowHide, (int)kb::hitTest(kb::Layer::Lower, sh.x + 5, sh.y + 5).kind);
    TEST_ASSERT_EQUAL((int)kb::KeyKind::None, (int)kb::hitTest(kb::Layer::Lower, 100, 20).kind);
}

void test_kb_all_printable_ascii_reachable()
{
    bool seen[128] = {false};
    const kb::Layer layers[4] = {kb::Layer::Lower, kb::Layer::Upper, kb::Layer::Sym1, kb::Layer::Sym2};
    for (kb::Layer l : layers) {
        for (int i = 0; i < kb::kKeyCount; ++i) {
            kb::Key k = kb::key(l, i);
            if (k.kind == kb::KeyKind::Char || k.kind == kb::KeyKind::Space) {
                TEST_ASSERT_TRUE(k.ch >= 0x20 && k.ch <= 0x7E);
                seen[(int)k.ch] = true;
            }
        }
    }
    for (int c = 0x20; c <= 0x7E; ++c) {
        if (!seen[c]) { TEST_FAIL_MESSAGE("printable ASCII character not on any layer"); }
    }
}

void test_kb_shift_once_caps_and_layers()
{
    kb::Keyboard k;
    k.begin(kb::Mode::Password);
    kb::Key shift = kb::key(kb::Layer::Lower, 19);
    kb::Key a = kb::key(kb::Layer::Lower, 10);
    TEST_ASSERT_EQUAL((int)kb::KeyKind::Shift, (int)shift.kind);

    // One-shot: one upper-case letter, then back to lower case.
    k.press(shift, 1000);
    TEST_ASSERT_EQUAL((int)kb::Layer::Upper, (int)k.layer());
    uint8_t ch = k.press(kb::key(k.layer(), 10), 2000);
    TEST_ASSERT_TRUE(ch & kb::Keyboard::kKeys);
    TEST_ASSERT_EQUAL_STRING("A", k.text());
    TEST_ASSERT_EQUAL((int)kb::Layer::Lower, (int)k.layer());

    // Double Shift = Caps Lock, stays on.
    k.press(shift, 3000);
    k.press(shift, 3000 + kb::kCapsWindowMs);
    TEST_ASSERT_EQUAL((int)kb::ShiftState::Caps, (int)k.shift());
    k.press(kb::key(k.layer(), 10), 4000);
    k.press(kb::key(k.layer(), 10), 4100);
    TEST_ASSERT_EQUAL_STRING("AAA", k.text());
    k.press(shift, 5000);
    TEST_ASSERT_EQUAL((int)kb::ShiftState::Off, (int)k.shift());

    // Slow second Shift tap: off again, not Caps.
    k.press(shift, 6000);
    k.press(shift, 6000 + kb::kCapsWindowMs + 1);
    TEST_ASSERT_EQUAL((int)kb::ShiftState::Off, (int)k.shift());

    // Symbols: toggle key, Shift switches symbol pages, letters come back in lower case.
    kb::Key toggle = kb::key(kb::Layer::Lower, 29);
    k.press(toggle, 7000);
    TEST_ASSERT_EQUAL((int)kb::Layer::Sym1, (int)k.layer());
    k.press(shift, 7100);
    TEST_ASSERT_EQUAL((int)kb::Layer::Sym2, (int)k.layer());
    k.press(kb::key(k.layer(), 0), 7200);   // '1', symbol layers stay
    TEST_ASSERT_EQUAL((int)kb::Layer::Sym2, (int)k.layer());
    k.press(toggle, 7300);
    TEST_ASSERT_EQUAL((int)kb::Layer::Lower, (int)k.layer());
    k.press(a, 7400);
    TEST_ASSERT_EQUAL_STRING("AAA1a", k.text());
}

void test_kb_length_limits_and_ok()
{
    kb::Keyboard k;
    kb::Key x = kb::key(kb::Layer::Lower, 21);   // 'x'
    kb::Key bksp = kb::key(kb::Layer::Lower, 27);
    kb::Key ok = kb::key(kb::Layer::Lower, 31);
    TEST_ASSERT_EQUAL((int)kb::KeyKind::Ok, (int)ok.kind);

    k.begin(kb::Mode::Password);
    for (int i = 0; i < 7; ++i) { k.press(x, 0); }
    TEST_ASSERT_FALSE(k.okEnabled());
    TEST_ASSERT_EQUAL(0, k.press(ok, 0));
    uint8_t ch = k.press(x, 0);
    TEST_ASSERT_TRUE(ch & kb::Keyboard::kOkKey);   // 8 chars: OK becomes enabled
    TEST_ASSERT_TRUE(k.okEnabled());
    TEST_ASSERT_EQUAL(kb::Keyboard::kDone, k.press(ok, 0));
    for (int i = 0; i < 70; ++i) { k.press(x, 0); }
    TEST_ASSERT_EQUAL(63, k.length());
    TEST_ASSERT_EQUAL(0, k.press(x, 0) & kb::Keyboard::kText);
    k.press(bksp, 0);
    TEST_ASSERT_EQUAL(62, k.length());

    k.begin(kb::Mode::Ssid);
    TEST_ASSERT_FALSE(k.okEnabled());
    k.press(x, 0);
    TEST_ASSERT_TRUE(k.okEnabled());
    for (int i = 0; i < 40; ++i) { k.press(x, 0); }
    TEST_ASSERT_EQUAL(32, k.length());
    k.press(bksp, 0);
    for (int i = 0; i < 40; ++i) { k.press(bksp, 0); }
    TEST_ASSERT_EQUAL(0, k.length());

    // Hidden network: empty = open network, 1..7 not allowed.
    k.begin(kb::Mode::PasswordOrOpen);
    TEST_ASSERT_TRUE(k.okEnabled());
    k.press(x, 0);
    TEST_ASSERT_FALSE(k.okEnabled());

    // Show/Hide only changes the field; Cancel always works.
    TEST_ASSERT_TRUE(k.shown());
    TEST_ASSERT_EQUAL(kb::Keyboard::kField, k.press(kb::hitTest(kb::Layer::Lower, 810, 50), 0));
    TEST_ASSERT_FALSE(k.shown());
    TEST_ASSERT_EQUAL(kb::Keyboard::kCancel, k.press(kb::key(kb::Layer::Lower, 28), 0));
}

void test_view_traits()
{
    TEST_ASSERT_FALSE(layout::traits(View::Home).immediate_taps);
    TEST_ASSERT_TRUE(layout::traits(View::Wifi).double_tap_home);
    ViewTraits t = layout::traits(View::Keyboard);
    TEST_ASSERT_TRUE(t.immediate_taps);
    TEST_ASSERT_FALSE(t.double_tap_home);
    TEST_ASSERT_FALSE(t.sidebar);
    // No sidebar hits on the keyboard.
    UiShape ui;
    ui.view = View::Keyboard;
    Rect s = layout::syncButton();
    TEST_ASSERT_EQUAL((int)Target::None, (int)layout::hitTest(ui, s.x + 5, s.y + 5).target);
    ui.view = View::Settings;
    Rect w = layout::wifiButton();
    TEST_ASSERT_EQUAL((int)Target::Wifi, (int)layout::hitTest(ui, w.x + 5, w.y + 5).target);
    TEST_ASSERT_TRUE(w.x + w.w <= layout::kSidebarX);
    TEST_ASSERT_TRUE(w.x >= layout::autoSyncButton().x + layout::autoSyncButton().w);
}

// ---- Wi-Fi list ----
// Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None

NetInfo net(const char* ssid, int rssi, WifiSecurity sec = WifiSecurity::Psk)
{
    NetInfo n;
    strncpy(n.ssid, ssid, sizeof(n.ssid) - 1);
    n.rssi = (int8_t)rssi;
    n.security = sec;
    return n;
}

void test_wifi_merge_and_sort()
{
    NetInfo n[7] = {
        net("home", -70), net("", -40), net("cafe", -60, WifiSecurity::Open), net("home", -50),
        net("b", -80), net("a", -80), net("home", -90),
    };
    int c = mergeNetworks(n, 7);
    TEST_ASSERT_EQUAL(4, c);
    TEST_ASSERT_EQUAL_STRING("home", n[0].ssid);
    TEST_ASSERT_EQUAL(-50, n[0].rssi);
    TEST_ASSERT_EQUAL_STRING("cafe", n[1].ssid);
    TEST_ASSERT_EQUAL((int)WifiSecurity::Open, (int)n[1].security);
    TEST_ASSERT_EQUAL_STRING("a", n[2].ssid);   // equal RSSI: by name
    TEST_ASSERT_EQUAL_STRING("b", n[3].ssid);
    TEST_ASSERT_EQUAL(0, mergeNetworks(n, 0));
}

void test_wifi_bars_and_printable()
{
    TEST_ASSERT_EQUAL(4, signalBars(-40));
    TEST_ASSERT_EQUAL(3, signalBars(-60));
    TEST_ASSERT_EQUAL(2, signalBars(-70));
    TEST_ASSERT_EQUAL(1, signalBars(-80));
    TEST_ASSERT_EQUAL(0, signalBars(-95));
    char out[8];
    printableSsid("ab\x01\xC3\xA9z", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("ab???z", out);
    printableSsid("abcdefghij", out, sizeof(out));
    TEST_ASSERT_EQUAL_STRING("abcdefg", out);
}

void test_wifi_sync_candidates_order()
{
    Credentials c[2];
    TEST_ASSERT_EQUAL(2, syncCandidates("home", "pw1", "hotspot", "pw2", c));
    TEST_ASSERT_EQUAL_STRING("home", c[0].ssid);
    TEST_ASSERT_EQUAL_STRING("pw1", c[0].password);
    TEST_ASSERT_EQUAL_STRING("hotspot", c[1].ssid);
    TEST_ASSERT_EQUAL(1, syncCandidates("", "", "hotspot", "pw2", c));
    TEST_ASSERT_EQUAL_STRING("hotspot", c[0].ssid);
    TEST_ASSERT_EQUAL(1, syncCandidates("hotspot", "new", "hotspot", "old", c));   // same SSID: saved wins
    TEST_ASSERT_EQUAL_STRING("new", c[0].password);
    TEST_ASSERT_EQUAL(1, syncCandidates("home", nullptr, "", "", c));
    TEST_ASSERT_EQUAL_STRING("", c[0].password);
    TEST_ASSERT_EQUAL(0, syncCandidates(nullptr, nullptr, nullptr, nullptr, c));
}

void test_wifi_layout_hits()
{
    wifilayout::Shape s;
    s.rows = 2;
    Rect r1 = wifilayout::listRow(1);
    TEST_ASSERT_TRUE(r1.h >= 64);
    wifilayout::Hit h = wifilayout::hitTest(s, r1.x + 10, r1.y + 10);
    TEST_ASSERT_EQUAL((int)wifilayout::Target::Network, (int)h.target);
    TEST_ASSERT_EQUAL(1, h.index);
    Rect r2 = wifilayout::listRow(2);
    TEST_ASSERT_EQUAL((int)wifilayout::Target::None, (int)wifilayout::hitTest(s, r2.x + 10, r2.y + 10).target);
    Rect f = wifilayout::forgetButton();
    TEST_ASSERT_EQUAL((int)wifilayout::Target::None, (int)wifilayout::hitTest(s, f.x + 5, f.y + 5).target);
    s.can_forget = true;
    TEST_ASSERT_EQUAL((int)wifilayout::Target::Forget, (int)wifilayout::hitTest(s, f.x + 5, f.y + 5).target);
    Rect last = wifilayout::listRow(wifilayout::kRowsPerPage - 1);
    TEST_ASSERT_TRUE(last.y + last.h <= 540);
    Rect d = wifilayout::downButton();
    TEST_ASSERT_TRUE(d.y + d.h <= 540 && d.x + d.w <= layout::kSidebarX);
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
    RUN_TEST(test_kb_keys_tile_area_and_are_big_enough);
    RUN_TEST(test_kb_all_printable_ascii_reachable);
    RUN_TEST(test_kb_shift_once_caps_and_layers);
    RUN_TEST(test_kb_length_limits_and_ok);
    RUN_TEST(test_view_traits);
    RUN_TEST(test_wifi_merge_and_sort);
    RUN_TEST(test_wifi_bars_and_printable);
    RUN_TEST(test_wifi_sync_candidates_order);
    RUN_TEST(test_wifi_layout_hits);
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
