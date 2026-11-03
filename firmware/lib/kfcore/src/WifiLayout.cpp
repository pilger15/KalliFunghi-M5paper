/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "WifiLayout.h"

namespace kf {
namespace wifilayout {

namespace {

constexpr int kListY = 100;
constexpr int kListW = 560;
constexpr int kBtnX = 580;
constexpr int kBtnW = 240;
constexpr int kBtnH = 80;
constexpr int kBtnPitch = 86;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
Rect mk(int x, int y, int w, int h)
{
    Rect r;
    r.x = (int16_t)x;
    r.y = (int16_t)y;
    r.w = (int16_t)w;
    r.h = (int16_t)h;
    return r;
}

Rect button(int i) { return mk(kBtnX, kListY + i * kBtnPitch, kBtnW, kBtnH); }

} // namespace

Rect header() { return mk(0, 0, layout::kSidebarX, kListY - 4); }
Rect listRow(int i) { return mk(0, kListY + i * kRowH, kListW, kRowH); }
Rect scanButton() { return button(0); }
Rect enterNameButton() { return button(1); }
Rect forgetButton() { return button(2); }
Rect upButton() { return button(3); }
Rect downButton() { return button(4); }
Rect syncNowButton() { return mk(kBtnX, 8, kBtnW, kBtnH); }

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
Hit hitTest(const Shape& s, int x, int y)
{
    Hit h;
    for (int i = 0; i < s.rows && i < kRowsPerPage; ++i) {
        if (listRow(i).contains(x, y)) { h.target = Target::Network; h.index = i; return h; }
    }
    if (scanButton().contains(x, y)) { h.target = Target::Scan; }
    else if (enterNameButton().contains(x, y)) { h.target = Target::EnterName; }
    else if (forgetButton().contains(x, y) && s.can_forget) { h.target = Target::Forget; }
    else if (upButton().contains(x, y) && s.can_up) { h.target = Target::PageUp; }
    else if (downButton().contains(x, y) && s.can_down) { h.target = Target::PageDown; }
    else if (syncNowButton().contains(x, y) && s.sync_now) { h.target = Target::SyncNow; }
    return h;
}

} // namespace wifilayout
} // namespace kf
