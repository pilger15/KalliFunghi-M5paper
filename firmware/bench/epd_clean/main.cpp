/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// One-off e-ink cleaning for image retention ("burned-in" pixels) left by an
// earlier program. Per round:
//   3x the IT8951 INIT waveform (about 2 s each, the strongest clear),
//   10x full black / full white with GC16, 5x fast DU flashes, 1x INIT,
// then a gray ramp so leftover marks are easy to spot, plus the panel VCOM.
// Flash, let it run (about 1 min per round), tap for another round, then flash
// the firmware again.
//
// M5GFX keeps the INIT waveform private (Panel_IT8951::_update_raw_area). This
// bench sketch reaches it by including the panel header with private -> public
// first. That is acceptable only in this one-off tool, never in the firmware.

#define private public
#include <lgfx/v1/panel/Panel_IT8951.hpp>
#undef private

#include <M5Unified.h>

namespace {

constexpr int kInitPerRound = 3;
constexpr int kBlackWhiteCycles = 10;

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
lgfx::Panel_IT8951* panel()
{
    return static_cast<lgfx::Panel_IT8951*>(M5.Display.getPanel());
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Full-screen refresh with the INIT waveform (drives every pixel through a full
// black/white sequence and ends white, regardless of the buffer content).
void initWaveform()
{
    auto p = panel();
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    M5.Display.fillScreen(TFT_WHITE);   // marks the whole screen as the update area
    M5.Display.startWrite();
    auto range = p->_range_new;
    p->_update_raw_area(lgfx::Panel_IT8951::UPDATE_MODE_INIT);
    p->_range_old = range;
    p->_range_new.top = INT16_MAX;
    p->_range_new.left = INT16_MAX;
    p->_range_new.right = 0;
    p->_range_new.bottom = 0;
    M5.Display.endWrite();
    M5.Display.waitDisplay();
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void flash(uint32_t color, epd_mode_t mode)
{
    M5.Display.setEpdMode(mode);
    M5.Display.fillScreen(color);
    M5.Display.display();
    M5.Display.waitDisplay();
}

/* Edited by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void cleanRound(int round)
{
    auto& d = M5.Display;
    for (int i = 0; i < kInitPerRound; ++i) { initWaveform(); }
    for (int i = 0; i < kBlackWhiteCycles; ++i) {
        flash(TFT_BLACK, epd_mode_t::epd_quality);
        flash(TFT_WHITE, epd_mode_t::epd_quality);
    }
    for (int i = 0; i < 5; ++i) {
        flash(TFT_BLACK, epd_mode_t::epd_fast);
        flash(TFT_WHITE, epd_mode_t::epd_fast);
    }
    initWaveform();

    // 16-step gray ramp plus status.
    d.setEpdMode(epd_mode_t::epd_quality);
    int w = d.width() / 16;
    for (int i = 0; i < 16; ++i) {
        uint8_t g = (uint8_t)(i * 17);
        d.fillRect(i * w, 0, w, d.height() / 2, d.color888(g, g, g));
    }
    uint16_t vcom = panel()->getVCOM();
    char buf[96];
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    d.setFont(&fonts::FreeSans12pt7b);
    d.setTextDatum(textdatum_t::middle_center);
    snprintf(buf, sizeof(buf), "Cleaning round %d done (INIT + GC16).", round);
    d.drawString(buf, d.width() / 2, d.height() * 5 / 8);
    snprintf(buf, sizeof(buf), "Panel VCOM: -%u.%02u V (compare with the label on the panel ribbon)",
             vcom / 1000, (vcom % 1000) / 10);
    d.drawString(buf, d.width() / 2, d.height() * 6 / 8);
    d.drawString("Tap for another round, else flash the firmware.", d.width() / 2, d.height() * 7 / 8);
    d.display();
    d.waitDisplay();
}

int g_round = 0;
uint16_t g_vcom_before = 0;
uint16_t g_vcom_after = 0;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// Shows the VCOM readings for 5 s before the cleaning starts.
void showVcom()
{
    auto& d = M5.Display;
    char buf[96];
    d.setEpdMode(epd_mode_t::epd_quality);
    d.fillScreen(TFT_WHITE);
    d.setTextColor(TFT_BLACK, TFT_WHITE);
    d.setFont(&fonts::FreeSans18pt7b);
    d.setTextDatum(textdatum_t::middle_center);
    snprintf(buf, sizeof(buf), "VCOM before: -%u.%02u V", g_vcom_before / 1000, (g_vcom_before % 1000) / 10);
    d.drawString(buf, d.width() / 2, d.height() / 3);
    snprintf(buf, sizeof(buf), "VCOM after setting -2.30 V: -%u.%02u V", g_vcom_after / 1000, (g_vcom_after % 1000) / 10);
    d.drawString(buf, d.width() / 2, d.height() / 2);
    d.setFont(&fonts::FreeSans12pt7b);
    d.drawString("Cleaning starts in 5 s.", d.width() / 2, d.height() * 2 / 3);
    d.display();
    d.waitDisplay();
    delay(5000);
}

} // namespace

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void setup()
{
    auto cfg = M5.config();
    M5.begin(cfg);
    M5.Display.setRotation(1);
    M5.Display.setAutoDisplay(false);
    // M5EPD always set -2.30 V; M5GFX sets no VCOM unless configured.
    g_vcom_before = panel()->getVCOM();
    panel()->setVCOM(2300);
    g_vcom_after = panel()->getVCOM();
    showVcom();
    cleanRound(++g_round);
}

/* Edited by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void loop()
{
    M5.update();
    if (M5.Touch.getCount() && M5.Touch.getDetail().wasClicked()) { cleanRound(++g_round); }
    M5.delay(20);
}
