/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "Ui.h"

#include <M5Unified.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "TimeUtil.h"
#include "config.h"

namespace kf {

namespace {

constexpr uint32_t kBlack = TFT_BLACK;
constexpr uint32_t kWhite = TFT_WHITE;

const char* const kTitles[kFieldCount] = {"CO2", "Temperature", "Humidity", "Light"};
const char* const kUnits[kFieldCount] = {"ppm", "degC", "%RH", "lux"};
const char* const kWeekdays[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void formatValue(int field, float v, char* buf, size_t len)
{
    switch ((PlotField)field) {
    case PlotField::Co2: snprintf(buf, len, "%.0f", v); break;
    case PlotField::Temp: snprintf(buf, len, "%.1f", v); break;
    case PlotField::Rh: snprintf(buf, len, "%.0f", v); break;
    default: snprintf(buf, len, (v < 10.0f) ? "%.1f" : "%.0f", v); break;
    }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// Local time offset (seconds) at utc, from the TZ rules.
int64_t localOffset(int64_t utc)
{
    time_t t = (time_t)utc;
    struct tm lt;
    localtime_r(&t, &lt);
    Civil c{lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday, lt.tm_hour, lt.tm_min, lt.tm_sec};
    return utcFromCivil(c) - utc;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
uint32_t tickStep(PlotRange r)
{
    switch (r) {
    case PlotRange::H1: return 15 * 60;
    case PlotRange::H6: return 3600;
    case PlotRange::H24: return 4 * 3600;
    default: return 86400;
    }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void text(const char* s, int x, int y, const lgfx::IFont* font, textdatum_t datum = textdatum_t::top_left)
{
    auto& d = M5.Display;
    d.setFont(font);
    d.setTextDatum(datum);
    d.drawString(s, x, y);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void fnv(uint32_t& h, const void* p, size_t n)
{
    const uint8_t* b = static_cast<const uint8_t*>(p);
    for (size_t i = 0; i < n; ++i) {
        h ^= b[i];
        h *= 16777619u;
    }
}

template <typename T>
void mix(uint32_t& h, const T& v) { fnv(h, &v, sizeof(v)); }

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
Rect Ui::graphRect(const Rect& tile)
{
    Rect g;
    g.x = tile.x + 62;
    g.y = tile.y + 72;
    g.w = tile.w - 62 - 14;
    g.h = tile.h - 72 - 34;
    return g;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
UiShape Ui::shape(const UiModel& m)
{
    UiShape s;
    s.view = m.view;
    s.cal_box = m.box && m.view != View::Settings;
    s.box_buttons = m.box_buttons;
    s.cal_manual = m.cal_manual;
    s.cal_ready = !m.calibrating;
    return s;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
uint32_t Ui::signature(const UiModel& m)
{
    uint32_t h = 2166136261u;
    mix(h, m.view);
    mix(h, m.plot);
    mix(h, m.range);
    mix(h, m.user_mode);
    mix(h, m.eff_mode);
    mix(h, m.interactive);
    mix(h, m.usb);
    int64_t minute = m.utc / 60;
    mix(h, minute);
    mix(h, m.clock_valid);
    int64_t sync_hours = m.last_sync_utc ? (m.utc - m.last_sync_utc) / 3600 : -1;
    mix(h, sync_hours);
    mix(h, m.battery_pct);
    mix(h, m.sd_problem);
    mix(h, m.cal_due_mark);
    mix(h, m.box);
    mix(h, m.box_buttons);
    mix(h, m.cal_manual);
    mix(h, m.calibrating);
    mix(h, m.cal_countdown_s);
    fnv(h, m.message, strlen(m.message));
    mix(h, m.auto_sync);
    mix(h, m.temp_offset);
    mix(h, m.last_cal_utc);
    for (int f = 0; f < kFieldCount; ++f) {
        mix(h, m.sensor_ok[f]);
        mix(h, m.has_current[f]);
        char v[16] = "";
        if (m.has_current[f]) { formatValue(f, m.current[f], v, sizeof(v)); }
        fnv(h, v, strlen(v));
    }
    mix(h, m.data_version);
    return h;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void Ui::drawButton(const Rect& r, const char* label, bool selected, bool enabled)
{
    auto& d = M5.Display;
    if (selected) {
        d.fillRoundRect(r.x, r.y, r.w, r.h, 10, kBlack);
        d.setTextColor(kWhite, kBlack);
    } else {
        d.fillRoundRect(r.x, r.y, r.w, r.h, 10, kWhite);
        d.drawRoundRect(r.x, r.y, r.w, r.h, 10, kBlack);
        if (enabled) { d.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 9, kBlack); }
        d.setTextColor(kBlack, kWhite);
    }
    text(label, r.x + r.w / 2, r.y + r.h / 2, &fonts::FreeSans12pt7b, textdatum_t::middle_center);
    d.setTextColor(kBlack, kWhite);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Ui::draw(const UiModel& m)
{
    auto& d = M5.Display;
    d.fillScreen(kWhite);
    d.setTextColor(kBlack, kWhite);

    bool box = m.box && m.view != View::Settings;
    switch (m.view) {
    case View::Home:
        for (int i = 0; i < kFieldCount; ++i) { drawPlot(m, i, layout::homeTile(i, box), false); }
        break;
    case View::Plot:
        drawPlot(m, m.plot, layout::plotArea(box), true);
        break;
    default:
        drawSettings(m);
        break;
    }
    if (box) { drawBox(m); }
    drawSidebar(m);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Ui::drawSidebar(const UiModel& m)
{
    auto& d = M5.Display;
    Rect s = layout::sidebar();
    d.drawFastVLine(s.x, 0, s.h, kBlack);
    d.drawFastVLine(s.x + 1, 0, s.h, kBlack);
    int cx = s.x + s.w / 2;
    char buf[32];

    struct tm lt;
    time_t t = (time_t)m.utc;
    localtime_r(&t, &lt);
    snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min);
    text(buf, cx, 10, &fonts::FreeSansBold18pt7b, textdatum_t::top_center);
    snprintf(buf, sizeof(buf), "%s %02d.%02d.", kWeekdays[lt.tm_wday], lt.tm_mday, lt.tm_mon + 1);
    text(buf, cx, 52, &fonts::FreeSans12pt7b, textdatum_t::top_center);

    if (m.usb) { snprintf(buf, sizeof(buf), "USB"); }
    else { snprintf(buf, sizeof(buf), "Bat %d%%", m.battery_pct); }
    text(buf, cx, 90, &fonts::FreeSans12pt7b, textdatum_t::top_center);

    const char* mode = (m.eff_mode == SamplingMode::Fast) ? "Fast"
                     : (m.eff_mode == SamplingMode::Ultra) ? "Ultra" : "Eco";
    snprintf(buf, sizeof(buf), "%s%s", mode, m.interactive ? " *" : "");
    text(buf, cx, 124, &fonts::FreeSansBold12pt7b, textdatum_t::top_center);

    int64_t age = m.last_sync_utc ? m.utc - m.last_sync_utc : -1;
    if (age < 0) { snprintf(buf, sizeof(buf), "Sync never"); }
    else if (age < 3600) { snprintf(buf, sizeof(buf), "Sync <1h"); }
    else if (age < 86400) { snprintf(buf, sizeof(buf), "Sync %lldh", (long long)(age / 3600)); }
    else { snprintf(buf, sizeof(buf), "Sync %lldd", (long long)(age / 86400)); }
    text(buf, cx, 160, &fonts::FreeSans9pt7b, textdatum_t::top_center);

    // Marks, one per line.
    int y = 190;
    if (!m.clock_valid || age < 0 || age > kSyncWarnAgeS) {
        text("! Clock", cx, y, &fonts::FreeSansBold12pt7b, textdatum_t::top_center);
        y += 30;
    }
    if (m.usb) {
        // Plug mark: body plus two pins.
        d.fillRect(cx - 12, y + 6, 24, 14, kBlack);
        d.fillRect(cx - 8, y, 4, 6, kBlack);
        d.fillRect(cx + 4, y, 4, 6, kBlack);
        d.fillRect(cx - 2, y + 20, 4, 6, kBlack);
        y += 34;
    }
    if (m.sd_problem) {
        text("! SD", cx, y, &fonts::FreeSansBold12pt7b, textdatum_t::top_center);
        y += 30;
    }
    if (m.cal_due_mark) {
        text("Cal due", cx, y, &fonts::FreeSansBold12pt7b, textdatum_t::top_center);
        y += 30;
    }

    drawButton(layout::syncButton(), "Sync", false);
    drawButton(layout::settingsButton(), "Settings", m.view == View::Settings);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Ui::drawPlot(const UiModel& m, int field, const Rect& r, bool large)
{
    auto& d = M5.Display;
    char buf[48];
    if (!large) { d.drawRect(r.x + 3, r.y + 3, r.w - 6, r.h - 6, kBlack); }

    snprintf(buf, sizeof(buf), "%s  %s", kTitles[field], kUnits[field]);
    text(buf, r.x + 14, r.y + 12, &fonts::FreeSans12pt7b);

    if (!m.sensor_ok[field]) {
        text("no sensor", r.x + r.w - 14, r.y + 8, &fonts::FreeSansBold18pt7b, textdatum_t::top_right);
    } else if (m.has_current[field]) {
        formatValue(field, m.current[field], buf, sizeof(buf));
        text(buf, r.x + r.w - 14, r.y + 6, &fonts::FreeSansBold24pt7b, textdatum_t::top_right);
    }

    Rect g = graphRect(r);
    const ColumnBins* b = m.bins[field];
    if (!b || !b->any()) {
        text(m.sensor_ok[field] ? "no data yet" : "no sensor", g.x + g.w / 2, g.y + g.h / 2,
             &fonts::FreeSans12pt7b, textdatum_t::middle_center);
        return;
    }

    float lo = b->overallMin();
    float hi = b->overallMax();
    char smin[16];
    char smax[16];
    formatValue(field, lo, smin, sizeof(smin));
    formatValue(field, hi, smax, sizeof(smax));
    snprintf(buf, sizeof(buf), "min %s  max %s", smin, smax);
    text(buf, r.x + 14, r.y + 42, &fonts::FreeSans9pt7b);

    float span = hi - lo;
    if (span < 1e-3f) { span = (fabsf(hi) > 1.0f) ? fabsf(hi) * 0.02f : 1.0f; }
    lo -= span * 0.05f;
    hi += span * 0.05f;

    d.drawRect(g.x, g.y, g.w, g.h, kBlack);
    formatValue(field, hi, buf, sizeof(buf));
    text(buf, g.x - 6, g.y, &fonts::FreeSans9pt7b, textdatum_t::top_right);
    formatValue(field, lo, buf, sizeof(buf));
    text(buf, g.x - 6, g.y + g.h, &fonts::FreeSans9pt7b, textdatum_t::bottom_right);

    // Time ticks on local time boundaries, dotted grid lines.
    int64_t t0 = b->t0();
    int64_t t1 = b->t1();
    int64_t step = tickStep(m.range);
    int64_t off = localOffset(t1);
    int64_t first = ((t0 + off) / step + 1) * step - off;
    for (int64_t t = first; t < t1; t += step) {
        int x = g.x + (int)((t - t0) * g.w / (t1 - t0));
        for (int y = g.y + 2; y < g.y + g.h; y += 6) { d.drawPixel(x, y, kBlack); }
        d.drawFastVLine(x, g.y + g.h, 5, kBlack);
        struct tm lt;
        time_t tt = (time_t)t;
        localtime_r(&tt, &lt);
        if (step < 86400) { snprintf(buf, sizeof(buf), "%02d:%02d", lt.tm_hour, lt.tm_min); }
        else { snprintf(buf, sizeof(buf), "%s", kWeekdays[lt.tm_wday]); }
        text(buf, x, g.y + g.h + 7, &fonts::FreeSans9pt7b, textdatum_t::top_center);
    }

    // Trace: per column a min..max span, joined to the previous column if close.
    auto ymap = [&](float v) {
        int y = g.y + g.h - 1 - (int)((v - lo) * (g.h - 2) / (hi - lo));
        return (y < g.y) ? (int)g.y : (y >= g.y + g.h ? g.y + g.h - 1 : y);
    };
    int prev_col = -1;
    int prev_y = 0;
    int cols = b->cols();
    for (int c = 0; c < cols && c < g.w; ++c) {
        if (!b->has(c)) { continue; }
        int y_hi = ymap(b->max(c));
        int y_lo = ymap(b->min(c));
        int y_mid = (y_hi + y_lo) / 2;
        int x = g.x + c;
        if (prev_col >= 0 && c - prev_col <= m.max_gap_cols) {
            d.drawLine(g.x + prev_col, prev_y, x, y_mid, kBlack);
            d.drawLine(g.x + prev_col, prev_y + 1, x, y_mid + 1, kBlack);
        }
        d.drawFastVLine(x, y_hi, (y_lo - y_hi) + 2, kBlack);
        if (large) { d.drawFastVLine(x + 1, y_hi, (y_lo - y_hi) + 2, kBlack); }
        prev_col = c;
        prev_y = y_mid;
    }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void Ui::drawSettings(const UiModel& m)
{
    static const char* const kRangeLabels[4] = {"1 h", "6 h", "24 h", "7 d"};
    static const char* const kModeLabels[3] = {"Ultra", "Eco", "Fast"};
    char buf[48];
    auto label = [&](int row, const char* s) {
        Rect r = layout::settingsRow(row);
        text(s, r.x, r.y + r.h / 2, &fonts::FreeSansBold12pt7b, textdatum_t::middle_left);
    };

    label(0, "Plot range");
    for (int i = 0; i < 4; ++i) { drawButton(layout::rangeButton(i), kRangeLabels[i], (int)m.range == i); }

    label(1, "Sampling");
    if (m.usb) {
        Rect r = layout::settingsRow(1);
        text("USB: Fast now", r.x, r.y + r.h - 4, &fonts::FreeSans9pt7b, textdatum_t::bottom_left);
    }
    for (int i = 0; i < 3; ++i) { drawButton(layout::modeButton(i), kModeLabels[i], (int)m.user_mode == i); }

    label(2, "Auto-sync");
    drawButton(layout::autoSyncButton(), m.auto_sync ? "On" : "Off", m.auto_sync);

    label(3, "Temp offset");
    drawButton(layout::offsetMinusButton(), "-", false);
    Rect v = layout::offsetValue();
    snprintf(buf, sizeof(buf), "%.1f degC", m.temp_offset);
    text(buf, v.x + v.w / 2, v.y + v.h / 2, &fonts::FreeSans12pt7b, textdatum_t::middle_center);
    drawButton(layout::offsetPlusButton(), "+", false);

    label(4, "CO2 calib.");
    if (m.last_cal_utc > 0) {
        struct tm lt;
        time_t t = (time_t)m.last_cal_utc;
        localtime_r(&t, &lt);
        snprintf(buf, sizeof(buf), "last %02d.%02d.%04d", lt.tm_mday, lt.tm_mon + 1, lt.tm_year + 1900);
        Rect r = layout::settingsRow(4);
        text(buf, r.x, r.y + r.h - 4, &fonts::FreeSans9pt7b, textdatum_t::bottom_left);
    }
    drawButton(layout::calibrateNowButton(), "Calibrate now (new batch)", false, !m.calibrating);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void Ui::drawBox(const UiModel& m)
{
    auto& d = M5.Display;
    Rect b = layout::calBox();
    d.fillRect(b.x, b.y, b.w, b.h, kWhite);
    d.drawFastHLine(b.x, b.y, b.w, kBlack);
    d.drawFastHLine(b.x, b.y + 1, b.w, kBlack);

    if (!m.box_buttons) {
        text(m.message, b.x + 14, b.y + b.h / 2, &fonts::FreeSans12pt7b, textdatum_t::middle_left);
        return;
    }
    char line1[48];
    char line2[48];
    if (m.calibrating) {
        snprintf(line1, sizeof(line1), "Calibrating, keep it outdoors.");
        snprintf(line2, sizeof(line2), "%d:%02d left", m.cal_countdown_s / 60, m.cal_countdown_s % 60);
    } else if (m.message[0]) {
        snprintf(line1, sizeof(line1), "%s", m.message);
        snprintf(line2, sizeof(line2), "Outdoors 30 min, then Calibrated.");
    } else if (m.cal_manual) {
        snprintf(line1, sizeof(line1), "Place the device outdoors 30 min,");
        snprintf(line2, sizeof(line2), "then press Calibrated.");
    } else {
        snprintf(line1, sizeof(line1), "CO2 calibration due. Place outdoors");
        snprintf(line2, sizeof(line2), "for 30 min, then press Calibrated.");
    }
    text(line1, b.x + 14, b.y + 14, &fonts::FreeSans9pt7b);
    text(line2, b.x + 14, b.y + 44, &fonts::FreeSans9pt7b);
    drawButton(layout::calConfirmButton(), "Calibrated", false, !m.calibrating);
    drawButton(layout::calSecondButton(), m.cal_manual || m.calibrating ? "Cancel" : "Postpone", false);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void Ui::drawLowBattery(const char* last_update)
{
    auto& d = M5.Display;
    d.fillScreen(kWhite);
    d.setTextColor(kBlack, kWhite);
    text("Battery low", kScreenW / 2, 170, &fonts::FreeSansBold24pt7b, textdatum_t::middle_center);
    char buf[64];
    snprintf(buf, sizeof(buf), "Last update %s", last_update);
    text(buf, kScreenW / 2, 260, &fonts::FreeSans18pt7b, textdatum_t::middle_center);
    text("Charge via USB, then press the power button.", kScreenW / 2, 340,
         &fonts::FreeSans12pt7b, textdatum_t::middle_center);
}

} // namespace kf
