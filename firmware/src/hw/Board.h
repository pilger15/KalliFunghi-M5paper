/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Board-level power pins and sleep helpers.
#pragma once

#include <stdint.h>

namespace kf {
namespace board {

// Call first in setup(): holds main power (GPIO2) through resets and tri-states
// U0TXD so it cannot feed the CP2104 when USB is absent.
void earlyInit();
// Timer-only light sleep (touch does not wake it). For short waits such as the
// VEML7700 integration time or the SCD41 single shot.
void nap(uint32_t ms);
// Light sleep until the timer or a touch (GT911 INT on GPIO36).
// Returns true if woken by touch.
bool sleepUntil(uint32_t ms);
// Cuts main power (GPIO2 low). Does not return on battery.
void powerOff();

} // namespace board
} // namespace kf
