/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "Board.h"

#include <Arduino.h>
#include <M5Unified.h>
#include <driver/gpio.h>
#include <esp_sleep.h>
#include "config.h"

namespace kf {
namespace board {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void earlyInit()
{
    // Main power hold. The hold keeps the level through a panic, watchdog or
    // brown-out reset, so such a reset on battery does not switch the board off.
    gpio_hold_dis((gpio_num_t)kPinMainPowerHold);
    pinMode(kPinMainPowerHold, OUTPUT);
    digitalWrite(kPinMainPowerHold, HIGH);
    gpio_hold_en((gpio_num_t)kPinMainPowerHold);

    pinMode(kPinUart0Tx, INPUT);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void nap(uint32_t ms)
{
    if (ms == 0) { return; }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_sleep_enable_timer_wakeup((uint64_t)ms * 1000ULL);
    esp_light_sleep_start();
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool sleepUntil(uint32_t ms)
{
    if (ms == 0) { return false; }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    // M5Unified arms ext0 on GPIO36 (low) and first waits until the GT911 releases INT.
    M5.Power.lightSleep((uint64_t)ms * 1000ULL, true);
    return esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0
        || esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void powerOff()
{
    gpio_hold_dis((gpio_num_t)kPinMainPowerHold);
    M5.Power.powerOff();
}

} // namespace board
} // namespace kf
