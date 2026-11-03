/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Bench: e-ink rest/wake cycles with the firmware's Display class. Found the brown-out
// when GPIO23 (EPD_PWR_EN) switched the panel back on (9 of 10 power-ons); now checks
// that sleep/wake without power switching survives.
// Trace over UART0 (USB, 115200). Each cycle: full refresh, rest(true), light sleep
// kOffMs, wake. A brown-out reboots the board; the reset count survives in RTC RAM.

#include <Arduino.h>
#include <M5Unified.h>
#include <SPI.h>
#include <esp_attr.h>
#include <esp_system.h>
#include "config.h"
#include "hw/Board.h"
#include "hw/DebugTrace.h"
#include "ui/Display.h"

namespace {

constexpr uint32_t kOffMs = 15000;

RTC_NOINIT_ATTR uint32_t g_magic;
RTC_NOINIT_ATTR uint32_t g_cycles;
RTC_NOINIT_ATTR uint32_t g_resets;

kf::Display g_display;

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void setup()
{
    kf::board::earlyInit();
    KF_TRACE_BEGIN();
    if (g_magic != 0x4b46u) {
        g_magic = 0x4b46u;
        g_cycles = 0;
        g_resets = 0;
    }
    esp_reset_reason_t r = esp_reset_reason();
    if (r == ESP_RST_BROWNOUT || r == ESP_RST_INT_WDT) { ++g_resets; }
    KF_TRACE("bench epd_power: reset reason %d, cycles %lu, bad resets %lu", (int)r,
             (unsigned long)g_cycles, (unsigned long)g_resets);

    auto cfg = M5.config();
    cfg.serial_baudrate = 0;
    cfg.clear_display = false;
    cfg.output_power = true;
    cfg.internal_imu = false;
    cfg.internal_mic = false;
    cfg.internal_spk = false;
    M5.begin(cfg);
    g_display.begin();
    SPI.begin(kf::kPinSpiSck, kf::kPinSpiMiso, kf::kPinSpiMosi, kf::kPinSdCs);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void loop()
{
    KF_TRACE("cycle %lu: wake", (unsigned long)g_cycles);
    uint32_t t0 = millis();
    g_display.beginFrame(true);
    M5.Display.fillScreen(TFT_WHITE);
    M5.Display.setTextColor(TFT_BLACK);
    M5.Display.setTextSize(4);
    M5.Display.setCursor(40, 40);
    M5.Display.printf("power cycle %lu, bad resets %lu", (unsigned long)g_cycles, (unsigned long)g_resets);
    g_display.endFrame();
    KF_TRACE("cycle %lu: drawn in %lu ms, panel asleep", (unsigned long)g_cycles, (unsigned long)(millis() - t0));
    g_display.rest(true);
    ++g_cycles;
    kf::board::nap(kOffMs);
}
