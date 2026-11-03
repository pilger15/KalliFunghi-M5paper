/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Wi-Fi page geometry and hit-testing (left of the sidebar). Pure, unit tested.
//
//   0..96     title, "Sync uses ...", status line        [Sync now] (after a good test)
//   100..532  network list, 6 rows of 72 px (x 0..560)   [Scan] [Enter name] [Forget] [Up] [Down]
#pragma once

#include "Layout.h"

namespace kf {
namespace wifilayout {

enum class Target : uint8_t { None, Network, Scan, EnterName, Forget, PageUp, PageDown, SyncNow };

struct Hit {
    Target target = Target::None;
    int index = 0;   // Network: row on the current page
};

// What the hit test needs to know about the page.
struct Shape {
    int rows = 0;            // networks on the current page
    bool can_forget = false; // a network is saved
    bool can_up = false;
    bool can_down = false;
    bool sync_now = false;   // Sync now button shown
};

constexpr int kRowsPerPage = 6;
constexpr int kRowH = 72;

Rect header();
Rect listRow(int i);
Rect scanButton();
Rect enterNameButton();
Rect forgetButton();
Rect upButton();
Rect downButton();
Rect syncNowButton();

Hit hitTest(const Shape& s, int x, int y);

} // namespace wifilayout
} // namespace kf
