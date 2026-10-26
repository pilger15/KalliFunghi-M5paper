/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "LightSensor.h"

namespace kf {

namespace {
constexpr uint8_t kAddr = 0x10;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
bool LightSensor::probe()
{
    wire_->beginTransmission(kAddr);
    return wire_->endTransmission() == 0;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool LightSensor::begin(TwoWire& wire)
{
    wire_ = &wire;
    present_ = probe() && veml_.begin(&wire);
    if (present_) {
        veml_.setGain(VEML7700_GAIN_1_8);
        veml_.setIntegrationTime(VEML7700_IT_100MS, false);
        veml_.enable(false);
    }
    return present_;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void LightSensor::start()
{
    started_ = false;
    if (!probe()) {
        present_ = false;
        return;
    }
    if (!present_) {
        // Plugged in again: full init.
        present_ = veml_.begin(wire_);
        if (!present_) { return; }
        veml_.setGain(VEML7700_GAIN_1_8);
        veml_.setIntegrationTime(VEML7700_IT_100MS, false);
    }
    veml_.enable(true);
    started_ = true;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool LightSensor::read(float& lux)
{
    if (!started_) { return false; }
    if (!probe()) {
        present_ = false;
        started_ = false;
        return false;
    }
    lux = veml_.readLux(VEML_LUX_CORRECTED_NOWAIT);
    veml_.enable(false);
    started_ = false;
    return lux >= 0.0f;
}

} // namespace kf
