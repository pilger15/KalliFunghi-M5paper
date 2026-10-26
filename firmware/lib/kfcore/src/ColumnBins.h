/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Downsampling for plots: one min/max pair per pixel column over a time window.
// Samples are added one by one, so long ranges can be streamed from SD files
// without holding them in RAM.
#pragma once

#include <stdint.h>

namespace kf {

class ColumnBins {
public:
    static constexpr int kMaxCols = 960;

    // Window [t0, t1) in UTC seconds mapped onto cols columns.
    void begin(int64_t t0, int64_t t1, int cols);
    void add(int64_t t, float v);

    int cols() const { return cols_; }
    bool has(int col) const { return count_[col] != 0; }
    float min(int col) const { return min_[col]; }
    float max(int col) const { return max_[col]; }
    bool any() const { return total_ != 0; }
    float overallMin() const { return omin_; }
    float overallMax() const { return omax_; }
    float last() const { return last_; }
    int64_t lastTime() const { return last_t_; }
    int64_t t0() const { return t0_; }
    int64_t t1() const { return t1_; }

private:
    int64_t t0_ = 0;
    int64_t t1_ = 1;
    int cols_ = 0;
    float min_[kMaxCols];
    float max_[kMaxCols];
    uint16_t count_[kMaxCols];
    uint32_t total_ = 0;
    float omin_ = 0.0f;
    float omax_ = 0.0f;
    float last_ = 0.0f;
    int64_t last_t_ = INT64_MIN;
};

} // namespace kf
