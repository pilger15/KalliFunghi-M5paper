/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "Display.h"

#include <M5Unified.h>
#include <lgfx/v1/panel/Panel_IT8951.hpp>
#include "config.h"
#include "hw/DebugTrace.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Display::begin()
{
    // Touch coordinates follow the display rotation (M5GFX), so hit-testing stays aligned.
    M5.Display.setRotation(kDisplayRotation);
    KF_TRACE("display %dx%d", (int)M5.Display.width(), (int)M5.Display.height());
    // M5GFX writes no VCOM unless configured; the panel then ran at 0 V and ghosted.
    // setVCOM stores the value, so every later re-init (wake from power-off) re-applies it.
    if (M5.getBoard() == m5::board_t::board_M5Paper) {
        auto* p = static_cast<lgfx::Panel_IT8951*>(M5.Display.getPanel());
        p->setVCOM(kEpdVcomMv);
        KF_TRACE("vcom -%u mV", (unsigned)p->getVCOM());
    }
    M5.Display.setAutoDisplay(false);
    state_ = State::Awake;
    force_full_ = true;
}

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Display::wake()
{
    if (state_ == State::Sleep) {
        // SYS_RUN only. Never M5.Display.wakeup(): it re-runs init(true), which cycles
        // GPIO23 and browns out the board.
        M5.Display.powerSaveOff();
        force_full_ = true;
        KF_TRACE("epd sleep -> run");
    } else if (state_ == State::Standby) {
        M5.Display.powerSaveOff();
    }
    state_ = State::Awake;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void Display::clean(int cycles)
{
    wake();
    M5.Display.setEpdMode(epd_mode_t::epd_quality);
    for (int i = 0; i < cycles; ++i) {
        M5.Display.fillScreen(TFT_BLACK);
        M5.Display.display();
        M5.Display.waitDisplay();
        M5.Display.fillScreen(TFT_WHITE);
        M5.Display.display();
        M5.Display.waitDisplay();
    }
    force_full_ = true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Display::beginFrame(bool full)
{
    wake();
    frame_full_ = full || force_full_ || partials_since_full_ >= kPartialsBeforeFull;
    M5.Display.setEpdMode(frame_full_ ? epd_mode_t::epd_quality : epd_mode_t::epd_fast);
    M5.Display.startWrite();
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void Display::endFrame()
{
    M5.Display.endWrite();
    M5.Display.display();
    M5.Display.waitDisplay();
    if (frame_full_) {
        partials_since_full_ = 0;
        force_full_ = false;
        ++full_count_;
    } else {
        ++partials_since_full_;
        ++partial_count_;
    }
}

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// GPIO23 (EPD power) stays high: switching it back on browns out the ESP32 (bench:
// 9 of 10 power-ons, logged as interrupt watchdog). Sleep is TCON_SLEEP only.
void Display::rest(bool deep)
{
    if (deep) {
        if (state_ == State::Sleep) { return; }
        if (state_ == State::Standby) { M5.Display.powerSaveOff(); }
        M5.Display.sleep();   // TCON_SLEEP; no setSleep(false) later (see wake)
        state_ = State::Sleep;
        force_full_ = true;
    } else {
        if (state_ != State::Awake) { return; }
        M5.Display.powerSaveOn();   // TCON_STANDBY
        state_ = State::Standby;
    }
}

} // namespace kf
