/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// E-ink power and refresh control around M5GFX's Panel_IT8951.
//
// Panel states:
//   Awake   - IT8951 running (during a refresh).
//   Standby - powerSaveOn() (TCON_STANDBY): no reset, frame buffer kept, SPI bus
//             usable. Used between interactive / USB refreshes.
//   Sleep   - sleep() (TCON_SLEEP), woken with SYS_RUN. Used between idle refreshes;
//             the next frame is a full GC16 redraw.
// GPIO23 (EPD_PWR_EN, M5GFX's "reset" pin) stays high the whole time: switching the
// panel power back on browns out the ESP32 (bench: 9 of 10 power-ons, logged as an
// interrupt-watchdog reset). The panel is therefore always powered and the shared
// SPI bus is usable for SD whenever no refresh is being pushed.
//
// Waveforms: GC16 (epd_quality) for full refreshes, DU (epd_fast) for partials.
// The mode is set before drawing: in epd_fast M5GFX writes 1-bit pixels.
//
// Regions (keyboard): M5GFX refreshes only the bounding box of what was drawn since
// the last display(), so beginRegion / endRegion around a small redraw updates just
// that area. epd_fastest is DU4 in M5GFX 0.2.34 (A2 is not reachable). endRegion
// can return without waiting for the waveform, so touch polling goes on meanwhile.
#pragma once

#include <stdint.h>

namespace kf {

class Display {
public:
    void begin();
    // Wakes the panel if needed and selects the waveform. Draw after this.
    // full=false may still become a full refresh (every 10th partial, after sleep).
    void beginFrame(bool full);
    bool frameIsFull() const { return frame_full_; }
    // Pushes the drawn frame and waits until the panel is done.
    void endFrame();
    enum class Wave : uint8_t { Fastest, Fast, Quality };   // DU4, DU, GC16
    // Region update: wakes the panel if needed, selects the waveform. Draw after this.
    void beginRegion(Wave w);
    // Refreshes the drawn area. wait=false returns as soon as the command is sent.
    void endRegion(bool wait);
    // True while the IT8951 still runs a waveform.
    bool busy() const;
    // After a frame: deep=true puts the controller to sleep, otherwise standby.
    void rest(bool deep);
    // Alternates full black and full white with GC16 to clear ghosting and image
    // retention (M5GFX does not expose the IT8951 INIT waveform). ~1 s per cycle.
    void clean(int cycles);
    uint32_t fullCount() const { return full_count_; }
    uint32_t partialCount() const { return partial_count_; }
    uint32_t regionCount() const { return region_count_; }

private:
    enum class State : uint8_t { Awake, Standby, Sleep };
    void wake();

    State state_ = State::Awake;
    bool frame_full_ = true;
    bool force_full_ = true;
    int partials_since_full_ = 0;
    uint32_t full_count_ = 0;
    uint32_t partial_count_ = 0;
    uint32_t region_count_ = 0;
};

} // namespace kf
