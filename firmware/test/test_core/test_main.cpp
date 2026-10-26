/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Host unit tests for lib/kfcore: pio test -e native

#include <math.h>
#include <string.h>
#include <unity.h>
#include "BatteryMonitor.h"
#include "CalReminder.h"
#include "ColumnBins.h"
#include "CsvFormat.h"
#include "Layout.h"
#include "PowerStateMachine.h"
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

void test_csv_day_path()
{
    char path[32];
    dayLogPath(utcFromCivil(Civil{2026, 1, 2, 23, 59, 59}), path, sizeof(path));
    TEST_ASSERT_EQUAL_STRING("/log/2026-01-02.csv", path);
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
    TEST_ASSERT_TRUE(c.y + c.h <= 540);
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

int runAll()
{
    UNITY_BEGIN();
    RUN_TEST(test_time_roundtrip);
    RUN_TEST(test_time_leap_day_and_epoch);
    RUN_TEST(test_csv_roundtrip);
    RUN_TEST(test_csv_missing_sensor_and_unsynced);
    RUN_TEST(test_csv_day_path);
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
    RUN_TEST(test_cal_reminder);
    RUN_TEST(test_battery_low_needs_consecutive_readings);
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
