/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "Layout.h"

namespace kf {
namespace layout {

namespace {

constexpr int16_t kRowY0 = 20;
constexpr int16_t kRowPitch = 100;
constexpr int16_t kRowH = 80;
constexpr int16_t kBtnX0 = 250;
constexpr int16_t kBtnW = 130;
constexpr int16_t kBtnGap = 12;

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

} // namespace

Rect plotArea(bool cal_box) { return mk(0, 0, kSidebarX, cal_box ? 540 - kCalBoxH : 540); }

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
Rect homeTile(int i, bool cal_box)
{
    Rect a = plotArea(cal_box);
    int w = a.w / 2;
    int h = a.h / 2;
    return mk((i % 2) * w, (i / 2) * h, w, h);
}

Rect sidebar() { return mk(kSidebarX, 0, 960 - kSidebarX, 540); }
Rect syncButton() { return mk(kSidebarX + 8, 372, 114, 76); }
Rect settingsButton() { return mk(kSidebarX + 8, 456, 114, 76); }
Rect calBox() { return mk(0, 540 - kCalBoxH, kSidebarX, kCalBoxH); }
Rect calConfirmButton() { return mk(470, 540 - kCalBoxH + 8, 170, kCalBoxH - 16); }
Rect calSecondButton() { return mk(650, 540 - kCalBoxH + 8, 170, kCalBoxH - 16); }
Rect settingsRow(int row) { return mk(20, kRowY0 + row * kRowPitch, 220, kRowH); }
Rect rangeButton(int i) { return mk(kBtnX0 + i * (kBtnW + kBtnGap), kRowY0, kBtnW, kRowH); }
Rect modeButton(int i) { return mk(kBtnX0 + i * (kBtnW + kBtnGap), kRowY0 + kRowPitch, kBtnW, kRowH); }
Rect autoSyncButton() { return mk(kBtnX0, kRowY0 + 2 * kRowPitch, 200, kRowH); }
Rect offsetMinusButton() { return mk(kBtnX0, kRowY0 + 3 * kRowPitch, 100, kRowH); }
Rect offsetValue() { return mk(kBtnX0 + 110, kRowY0 + 3 * kRowPitch, 150, kRowH); }
Rect offsetPlusButton() { return mk(kBtnX0 + 270, kRowY0 + 3 * kRowPitch, 100, kRowH); }
Rect calibrateNowButton() { return mk(kBtnX0, kRowY0 + 4 * kRowPitch, 560, kRowH); }

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
Hit hitTest(const UiShape& ui, int x, int y)
{
    Hit h;
    if (syncButton().contains(x, y)) { h.target = Target::Sync; return h; }
    if (settingsButton().contains(x, y)) { h.target = Target::Settings; return h; }
    if (sidebar().contains(x, y)) { return h; }

    if (ui.view == View::Settings) {
        for (int i = 0; i < 4; ++i) {
            if (rangeButton(i).contains(x, y)) { h.target = Target::Range; h.index = i; return h; }
        }
        for (int i = 0; i < 3; ++i) {
            if (modeButton(i).contains(x, y)) { h.target = Target::Mode; h.index = i; return h; }
        }
        if (autoSyncButton().contains(x, y)) { h.target = Target::AutoSync; return h; }
        if (offsetMinusButton().contains(x, y)) { h.target = Target::OffsetMinus; return h; }
        if (offsetPlusButton().contains(x, y)) { h.target = Target::OffsetPlus; return h; }
        if (calibrateNowButton().contains(x, y)) { h.target = Target::CalibrateNow; return h; }
        return h;
    }

    if (ui.cal_box && calBox().contains(x, y)) {
        if (!ui.box_buttons) { return h; }
        if (calConfirmButton().contains(x, y) && ui.cal_ready) { h.target = Target::CalConfirm; }
        else if (calSecondButton().contains(x, y)) {
            // While calibrating the second button cancels, also for a reminder box.
            h.target = (ui.cal_manual || !ui.cal_ready) ? Target::CalCancel : Target::CalPostpone;
        }
        return h;
    }

    if (ui.view == View::Home) {
        for (int i = 0; i < 4; ++i) {
            if (homeTile(i, ui.cal_box).contains(x, y)) { h.target = Target::PlotTile; h.index = i; return h; }
        }
        return h;
    }
    if (plotArea(ui.cal_box).contains(x, y)) { h.target = Target::PlotArea; }
    return h;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
bool actionable(const Hit& h)
{
    return h.target != Target::None && h.target != Target::PlotArea;
}

} // namespace layout
} // namespace kf
