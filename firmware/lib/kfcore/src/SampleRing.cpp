/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "SampleRing.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void SampleRing::push(const Sample& s)
{
    if (cap_ == 0) { return; }
    buf_[head_] = s;
    head_ = (head_ + 1) % cap_;
    if (size_ < cap_) { ++size_; }
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void SampleRing::rebaseUnsynced(int64_t delta_s)
{
    for (size_t i = 0; i < size_; ++i) {
        Sample& s = buf_[(head_ + cap_ - size_ + i) % cap_];
        if (s.flags & Sample::kUnsynced) {
            s.utc = (uint32_t)((int64_t)s.utc + delta_s);
            s.flags &= ~Sample::kUnsynced;
        }
    }
}

} // namespace kf
