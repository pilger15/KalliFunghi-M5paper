/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Fixed-capacity ring of samples. The caller provides the storage (PSRAM on the
// device, heap in tests), so this class never allocates.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "Types.h"

namespace kf {

class SampleRing {
public:
    SampleRing(Sample* storage, size_t capacity) : buf_(storage), cap_(capacity) {}

    void push(const Sample& s);
    void clear() { head_ = 0; size_ = 0; }
    size_t size() const { return size_; }
    size_t capacity() const { return cap_; }
    bool empty() const { return size_ == 0; }
    // 0 = oldest, size()-1 = newest.
    const Sample& at(size_t i) const { return buf_[(head_ + cap_ - size_ + i) % cap_]; }
    const Sample& newest() const { return at(size_ - 1); }
    // Shifts the timestamps of unsynced samples by delta_s and clears their flag.
    // Used once the first NTP sync tells how far off the clock was.
    void rebaseUnsynced(int64_t delta_s);

private:
    Sample* buf_;
    size_t cap_;
    size_t head_ = 0;   // next write position
    size_t size_ = 0;
};

} // namespace kf
