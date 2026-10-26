/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
#include "ColumnBins.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void ColumnBins::begin(int64_t t0, int64_t t1, int cols)
{
    t0_ = t0;
    t1_ = (t1 > t0) ? t1 : t0 + 1;
    cols_ = (cols < 1) ? 1 : (cols > kMaxCols ? kMaxCols : cols);
    for (int i = 0; i < cols_; ++i) { count_[i] = 0; }
    total_ = 0;
    last_t_ = INT64_MIN;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void ColumnBins::add(int64_t t, float v)
{
    if (t < t0_ || t >= t1_) { return; }
    int col = (int)((t - t0_) * cols_ / (t1_ - t0_));
    if (count_[col] == 0) {
        min_[col] = v;
        max_[col] = v;
    } else {
        if (v < min_[col]) { min_[col] = v; }
        if (v > max_[col]) { max_[col] = v; }
    }
    if (count_[col] < UINT16_MAX) { ++count_[col]; }
    if (total_ == 0) {
        omin_ = v;
        omax_ = v;
    } else {
        if (v < omin_) { omin_ = v; }
        if (v > omax_) { omax_ = v; }
    }
    ++total_;
    if (t >= last_t_) {
        last_t_ = t;
        last_ = v;
    }
}

} // namespace kf
