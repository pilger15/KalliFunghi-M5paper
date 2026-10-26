/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// USB presence. Method from config.h (kUsbMethod):
//  - BatteryHeuristic (default until the bench run): see UsbHeuristic.h for the
//    rules and the detection delays.
//  - Gpio3: the CP2104 TXD idles high only while USB powers it. Read with pull-down,
//    so an unpowered CP2104 is never fed through the pin.
// Hardware alternative (not fitted, your decision): VBUS divider to a free ADC pin.
#pragma once

#include <stdint.h>
#include "UsbHeuristic.h"

namespace kf {

class UsbDetect {
public:
    UsbDetect();
    void begin();
    // Call once per wake with the fixed-load-point battery voltage.
    bool update(uint32_t now_ms, int battery_mv);
    bool present() const { return present_; }

private:
    UsbHeuristic heuristic_;
    bool present_ = false;
};

} // namespace kf
