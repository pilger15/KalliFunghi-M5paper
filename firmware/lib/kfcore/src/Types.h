/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Shared value types. No Arduino headers (used by the native tests).
#pragma once

#include <stdint.h>

namespace kf {

enum class SamplingMode : uint8_t { Ultra = 0, Eco = 1, Fast = 2 };

enum class PlotRange : uint8_t { H1 = 0, H6 = 1, H24 = 2, D7 = 3 };

enum class PlotField : uint8_t { Co2 = 0, Temp = 1, Rh = 2, Lux = 3 };
constexpr int kFieldCount = 4;

inline uint32_t rangeSeconds(PlotRange r)
{
    switch (r) {
    case PlotRange::H1: return 3600UL;
    case PlotRange::H6: return 6UL * 3600;
    case PlotRange::H24: return 24UL * 3600;
    default: return 7UL * 86400;
    }
}

inline const char* modeName(SamplingMode m)
{
    switch (m) {
    case SamplingMode::Ultra: return "ultra";
    case SamplingMode::Fast: return "fast";
    default: return "eco";
    }
}

// One logged sample. Values are only meaningful when the matching flag is set.
struct Sample {
    enum Flags : uint8_t {
        kUnsynced = 1 << 0,  // UTC came from a clock that was never synced
        kScdValid = 1 << 1,  // co2, temp, rh valid
        kLuxValid = 1 << 2,
    };
    uint32_t utc = 0;        // seconds since 1970, UTC
    uint16_t co2 = 0;        // ppm
    int16_t temp_c100 = 0;   // degC * 100
    uint16_t rh_c100 = 0;    // %RH * 100
    float lux = 0.0f;
    uint8_t mode = 0;        // SamplingMode
    uint8_t flags = 0;
};

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
inline bool sampleValue(const Sample& s, PlotField f, float& out)
{
    switch (f) {
    case PlotField::Co2:
        if (!(s.flags & Sample::kScdValid)) { return false; }
        out = s.co2;
        return true;
    case PlotField::Temp:
        if (!(s.flags & Sample::kScdValid)) { return false; }
        out = s.temp_c100 / 100.0f;
        return true;
    case PlotField::Rh:
        if (!(s.flags & Sample::kScdValid)) { return false; }
        out = s.rh_c100 / 100.0f;
        return true;
    default:
        if (!(s.flags & Sample::kLuxValid)) { return false; }
        out = s.lux;
        return true;
    }
}

} // namespace kf
