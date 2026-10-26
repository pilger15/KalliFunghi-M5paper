/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// E-ink power and refresh control around M5GFX's Panel_IT8951.
//
// Panel states:
//   Awake   - IT8951 running (during a refresh).
//   Standby - powerSaveOn() (TCON_STANDBY): no reset, frame buffer kept, SPI bus
//             usable. Used between interactive / USB refreshes.
//   Off     - sleep() then GPIO23 (EPD_PWR_EN, M5GFX's "reset" pin) low. Used between
//             idle refreshes. Waking costs a re-init (~800 ms) and the IT8951 frame
//             buffer is lost, so the next frame is always a full GC16 redraw.
//             CS15 and BUSY27 are parked with pull-downs so the unpowered IT8951 is
//             not fed through them. SD access is not allowed in this state (the
//             unpowered IT8951 loads the shared MISO line).
//
// Waveforms: GC16 (epd_quality) for full refreshes, DU (epd_fast) for partials.
// The mode is set before drawing: in epd_fast M5GFX writes 1-bit pixels.
#pragma once

#include <stdint.h>

namespace kf {

class Display {
public:
    void begin();
    // Wakes the panel if needed and selects the waveform. Draw after this.
    // full=false may still become a full refresh (every 10th partial, after power-off).
    void beginFrame(bool full);
    bool frameIsFull() const { return frame_full_; }
    // Pushes the drawn frame and waits until the panel is done.
    void endFrame();
    // After a frame: deep=true powers the panel off, otherwise standby.
    void rest(bool deep);
    // True while the SPI bus may be used for SD (panel powered, not refreshing).
    bool busPowered() const { return state_ != State::Off; }
    // Powers the panel (standby) so the SD card can be used.
    void powerForSd();
    // Alternates full black and full white with GC16 to clear ghosting and image
    // retention (M5GFX does not expose the IT8951 INIT waveform). ~1 s per cycle.
    void clean(int cycles);
    uint32_t fullCount() const { return full_count_; }
    uint32_t partialCount() const { return partial_count_; }

private:
    enum class State : uint8_t { Awake, Standby, Off };
    void wake();

    State state_ = State::Awake;
    bool frame_full_ = true;
    bool force_full_ = true;
    int partials_since_full_ = 0;
    uint32_t full_count_ = 0;
    uint32_t partial_count_ = 0;
};

} // namespace kf
