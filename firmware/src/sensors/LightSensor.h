/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// VEML7700: shut down between reads (about 0.5 uA). Fixed gain 1/8 and 100 ms
// integration: 0.46 lux resolution, up to about 30 klux, enough for a grow box.
// Usage: start(), wait kVemlSettleMs (light sleep), read().
#pragma once

#include <Wire.h>
#include <Adafruit_VEML7700.h>

namespace kf {

class LightSensor {
public:
    bool begin(TwoWire& wire);
    bool present() const { return present_; }
    void start();
    // Reads the lux value and shuts the sensor down. False on I2C error.
    bool read(float& lux);

private:
    bool probe();

    Adafruit_VEML7700 veml_;
    TwoWire* wire_ = nullptr;
    bool present_ = false;
    bool started_ = false;
};

} // namespace kf
