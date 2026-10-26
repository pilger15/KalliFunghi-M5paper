/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "UsbDetect.h"

#include <Arduino.h>
#include "config.h"

namespace kf {

namespace {
const UsbHeuristic::Params kParams{
    kUsbEnterMv, kUsbLeaveMv, kUsbRiseMv, kUsbDropMv,
    kUsbWindowMs, kUsbHeuristicStepMs,
};
}

UsbDetect::UsbDetect() : heuristic_(kParams) {}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void UsbDetect::begin()
{
    pinMode(kPinUart0Tx, INPUT);
    pinMode(kPinUart0Rx, INPUT_PULLDOWN);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool UsbDetect::update(uint32_t now_ms, int battery_mv)
{
    if (kUsbMethod == UsbMethod::Gpio3) {
        present_ = digitalRead(kPinUart0Rx) == HIGH;
    } else {
        present_ = heuristic_.feed(now_ms, battery_mv);
    }
    return present_;
}

} // namespace kf
