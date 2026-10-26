/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Screen rendering. Draws a complete view from a Model; the App decides when.
// Black and white only (no gray), so DU partials and GC16 look the same.
#pragma once

#include <stdint.h>
#include "ColumnBins.h"
#include "Layout.h"
#include "Types.h"

namespace kf {

struct UiModel {
    View view = View::Home;
    int plot = 0;                    // full-screen plot field
    PlotRange range = PlotRange::H6;
    SamplingMode user_mode = SamplingMode::Eco;
    SamplingMode eff_mode = SamplingMode::Eco;
    bool interactive = false;
    bool usb = false;

    int64_t utc = 0;
    bool clock_valid = false;        // synced at least once and RTC not VL
    int64_t last_sync_utc = 0;
    int battery_pct = 0;
    bool sd_problem = false;
    bool cal_due_mark = false;

    // Bottom strip: calibration box or a message.
    bool box = false;
    bool box_buttons = false;
    bool cal_manual = false;
    bool calibrating = false;
    int cal_countdown_s = 0;
    char message[64] = "";

    bool auto_sync = true;
    float temp_offset = 0.0f;
    int64_t last_cal_utc = 0;

    bool sensor_ok[kFieldCount] = {false, false, false, false};
    const ColumnBins* bins[kFieldCount] = {nullptr, nullptr, nullptr, nullptr};
    bool has_current[kFieldCount] = {false, false, false, false};
    float current[kFieldCount] = {0, 0, 0, 0};
    int max_gap_cols = 1;            // connect points closer than this (columns)
    uint32_t data_version = 0;       // bumps when the bins change
};

class Ui {
public:
    void draw(const UiModel& m);
    void drawLowBattery(const char* last_update);
    // Hash of everything visible: equal hashes mean an unchanged screen.
    static uint32_t signature(const UiModel& m);
    // Graph rectangle inside a plot tile (columns = its width).
    static Rect graphRect(const Rect& tile);
    static UiShape shape(const UiModel& m);

private:
    void drawSidebar(const UiModel& m);
    void drawPlot(const UiModel& m, int field, const Rect& r, bool large);
    void drawSettings(const UiModel& m);
    void drawBox(const UiModel& m);
    void drawButton(const Rect& r, const char* label, bool selected, bool enabled = true);
};

} // namespace kf
