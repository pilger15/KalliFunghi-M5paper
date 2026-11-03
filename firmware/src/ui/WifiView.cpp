/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Low | Reviewed by: None */
// Wi-Fi page: network in use, scan results, Scan / Enter name / Forget / paging.
#include "Ui.h"

#include <M5Unified.h>
#include <stdio.h>

namespace kf {

namespace {

constexpr uint32_t kBlack = TFT_BLACK;
constexpr uint32_t kWhite = TFT_WHITE;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void text(const char* s, int x, int y, const lgfx::IFont* font, textdatum_t datum = textdatum_t::top_left)
{
    auto& d = M5.Display;
    d.setFont(font);
    d.setTextDatum(datum);
    d.drawString(s, x, y);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// Four bars of rising height, filled up to `bars`.
void drawBars(int x, int y_bottom, int bars)
{
    auto& d = M5.Display;
    for (int i = 0; i < 4; ++i) {
        int h = 8 + i * 8;
        int bx = x + i * 12;
        if (i < bars) { d.fillRect(bx, y_bottom - h, 8, h, kBlack); }
        else { d.drawRect(bx, y_bottom - h, 8, h, kBlack); }
    }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void drawLock(int x, int y)
{
    auto& d = M5.Display;
    d.drawRoundRect(x + 4, y, 14, 16, 6, kBlack);
    d.drawRoundRect(x + 5, y + 1, 12, 14, 5, kBlack);
    d.fillRect(x, y + 10, 22, 16, kBlack);
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
int WifiPageModel::rowsOnPage() const
{
    int first = page * wifilayout::kRowsPerPage;
    int left = count - first;
    if (left < 0) { return 0; }
    return left < wifilayout::kRowsPerPage ? left : wifilayout::kRowsPerPage;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
wifilayout::Shape WifiPageModel::shape() const
{
    wifilayout::Shape s;
    s.rows = rowsOnPage();
    s.can_forget = can_forget;
    s.can_up = page > 0;
    s.can_down = page + 1 < pages();
    s.sync_now = sync_now;
    return s;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void Ui::drawWifi(const UiModel& m)
{
    auto& d = M5.Display;
    if (!m.wifi) { return; }
    const WifiPageModel& w = *m.wifi;
    char buf[48];

    text("Wi-Fi", 14, 8, &fonts::FreeSansBold18pt7b);
    text(w.uses, 14, 46, &fonts::FreeSans9pt7b);
    text(w.status, 14, 70, &fonts::FreeSansBold9pt7b);
    if (w.sync_now) { drawButton(wifilayout::syncNowButton(), "Sync now", false); }
    d.drawFastHLine(0, wifilayout::header().h, layout::kSidebarX, kBlack);

    int rows = w.rowsOnPage();
    if (rows == 0) {
        Rect r = wifilayout::listRow(1);
        text(w.scanned ? "No networks found." : "Press Scan to list networks.", r.x + 14, r.y + r.h / 2,
             &fonts::FreeSans12pt7b, textdatum_t::middle_left);
    }
    for (int i = 0; i < rows; ++i) {
        const NetInfo& n = w.nets[w.page * wifilayout::kRowsPerPage + i];
        Rect r = wifilayout::listRow(i);
        d.drawFastHLine(r.x, r.y + r.h - 1, r.w, kBlack);
        printableSsid(n.ssid, buf, sizeof(buf));
        text(buf, r.x + 14, r.y + r.h / 2, &fonts::FreeSans12pt7b, textdatum_t::middle_left);
        int bx = r.x + r.w - 60;
        drawBars(bx, r.y + r.h / 2 + 16, signalBars(n.rssi));
        if (n.security == WifiSecurity::Psk) { drawLock(bx - 40, r.y + r.h / 2 - 13); }
        else if (n.security == WifiSecurity::Enterprise) {
            text("802.1X", bx - 12, r.y + r.h / 2, &fonts::FreeSans9pt7b, textdatum_t::middle_right);
        }
    }

    wifilayout::Shape s = w.shape();
    drawButton(wifilayout::scanButton(), "Scan", false);
    drawButton(wifilayout::enterNameButton(), "Enter name", false);
    drawButton(wifilayout::forgetButton(), "Forget", false, s.can_forget);
    snprintf(buf, sizeof(buf), "Up  %d/%d", w.page + 1, w.pages());
    drawButton(wifilayout::upButton(), buf, false, s.can_up);
    drawButton(wifilayout::downButton(), "Down", false, s.can_down);
}

} // namespace kf
