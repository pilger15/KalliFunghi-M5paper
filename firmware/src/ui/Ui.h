/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Screen rendering. Draws a complete view from a Model; the App decides when.
// Black and white only (no gray), so DU partials and GC16 look the same.
#pragma once

#include <stdint.h>
#include "ColumnBins.h"
#include "Keyboard.h"
#include "Layout.h"
#include "Types.h"
#include "WifiLayout.h"
#include "WifiList.h"

namespace kf {

// Yes / No question in the bottom strip.
enum class Ask : uint8_t { None, NewRun, CalNow };

// Wi-Fi page content, owned by the App. version bumps on every change.
struct WifiPageModel {
    NetInfo nets[kMaxNetworks];
    int count = 0;
    int page = 0;
    bool scanned = false;
    char uses[96] = "";      // "Sync uses: ..."
    char status[64] = "";
    bool sync_now = false;   // Sync now button (after a good connection test)
    bool can_forget = false;
    uint32_t version = 0;

    int pages() const { return count > 0 ? (count + wifilayout::kRowsPerPage - 1) / wifilayout::kRowsPerPage : 1; }
    int rowsOnPage() const;
    wifilayout::Shape shape() const;
};

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
    Ask ask = Ask::None;             // the strip asks this instead of the calibration box
    bool cal_manual = false;
    bool calibrating = false;
    int cal_countdown_s = 0;
    char message[64] = "";

    bool auto_sync = true;
    float temp_offset = 0.0f;
    int64_t last_cal_utc = 0;        // 0 = never calibrated
    int run_no = 0;                  // 0 = no run yet
    int64_t run_start_utc = 0;

    bool sensor_ok[kFieldCount] = {false, false, false, false};
    const ColumnBins* bins[kFieldCount] = {nullptr, nullptr, nullptr, nullptr};
    bool has_current[kFieldCount] = {false, false, false, false};
    float current[kFieldCount] = {0, 0, 0, 0};
    int max_gap_cols = 1;            // connect points closer than this (columns)
    uint32_t data_version = 0;       // bumps when the bins change
    const WifiPageModel* wifi = nullptr;   // View::Wifi
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

    // Keyboard (KeyboardView.cpp). Full screen, no sidebar. The parts are drawn on
    // their own so the App can refresh only the changed region.
    void drawKeyboard(const kb::Keyboard& k, const char* prompt);
    void drawKbField(const kb::Keyboard& k, const char* prompt);
    void drawKbKeys(const kb::Keyboard& k);
    void drawKbKey(const kb::Keyboard& k, int index, bool pressed);

private:
    void drawSidebar(const UiModel& m);
    void drawPlot(const UiModel& m, int field, const Rect& r, bool large);
    void drawSettings(const UiModel& m);
    void drawWifi(const UiModel& m);   // WifiView.cpp
    void drawBox(const UiModel& m);
    void drawButton(const Rect& r, const char* label, bool selected, bool enabled = true);
};

} // namespace kf
