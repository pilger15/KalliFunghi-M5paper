/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Screen geometry and hit-testing (960x540 landscape, sidebar on the right).
// Pure, so drawing and touch handling use the same rectangles and tests can check them.
#pragma once

#include <stdint.h>

namespace kf {

struct Rect {
    int16_t x = 0;
    int16_t y = 0;
    int16_t w = 0;
    int16_t h = 0;
    bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

enum class View : uint8_t { Home, Plot, Settings };

enum class Target : uint8_t {
    None,
    PlotTile,     // index 0..3 (home)
    PlotArea,     // full-screen plot body, no single-tap action
    Sync,
    Settings,
    Range,        // index 0..3
    Mode,         // index 0..2 (Ultra, Eco, Fast)
    AutoSync,
    OffsetMinus,
    OffsetPlus,
    CalibrateNow,
    CalConfirm,
    CalPostpone,
    CalCancel,
};

struct Hit {
    Target target = Target::None;
    int index = 0;
};

// What the hit test needs to know about the screen.
struct UiShape {
    View view = View::Home;
    bool cal_box = false;      // bottom strip visible (calibration box or a message)
    bool box_buttons = false;  // strip shows the calibration buttons
    bool cal_manual = false;   // box opened by "Calibrate now": Cancel instead of Postpone
    bool cal_ready = false;    // Calibrated is active (not already calibrating)
};

namespace layout {

constexpr int kSidebarX = 830;
constexpr int kCalBoxH = 80;

Rect plotArea(bool cal_box);
Rect homeTile(int i, bool cal_box);
Rect sidebar();
Rect syncButton();
Rect settingsButton();
Rect calBox();
Rect calConfirmButton();
Rect calSecondButton();     // Postpone or Cancel
Rect settingsRow(int row);  // label row, rows 0..4
Rect rangeButton(int i);
Rect modeButton(int i);
Rect autoSyncButton();
Rect offsetMinusButton();
Rect offsetValue();
Rect offsetPlusButton();
Rect calibrateNowButton();

Hit hitTest(const UiShape& ui, int x, int y);
// True if a single tap on this target does something (it then waits for the
// double-tap window before acting).
bool actionable(const Hit& h);

} // namespace layout
} // namespace kf
