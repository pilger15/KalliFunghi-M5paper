/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "BleProto.h"

#include <stdio.h>
#include <string.h>
#include "RunLog.h"

namespace kf {
namespace bleproto {

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Bitwise, no table: 1 KB less flash, and ~1 MB/s is far above the BLE rate.
uint32_t crc32Update(uint32_t crc, const uint8_t* p, size_t n)
{
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k) { crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u))); }
    }
    return ~crc;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool validPath(const char* rel)
{
    size_t n = strnlen(rel, kPathMax);
    if (n == 0 || n >= kPathMax) { return false; }
    if (strcmp(rel, "error.log") == 0 || strcmp(rel, "sync.csv") == 0) { return true; }
    for (size_t i = 0; i < n; ++i) {
        char c = rel[i];
        if (c < 0x21 || c > 0x7E || c == '\\') { return false; }
    }
    const char* slash = strchr(rel, '/');
    if (!slash || slash == rel) { return false; }
    const char* name = slash + 1;
    if (*name == '\0' || *name == '.' || strchr(name, '/') || strstr(rel, "..")) { return false; }
    char dir[kPathMax];
    size_t dn = (size_t)(slash - rel);
    memcpy(dir, rel, dn);
    dir[dn] = '\0';
    return parseRunNumber(dir) >= 0;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool parseRequest(const uint8_t* p, size_t n, Request& out, Status& status)
{
    status = kBadRequest;
    if (n == 0) { return false; }
    out = Request();
    out.op = (Op)p[0];
    switch (p[0]) {
    case kOpList:
    case kOpAbort:
        if (n != 1) { return false; }
        break;
    case kOpAck:
        if (n != 5) { return false; }
        out.offset = getU32(p + 1);
        break;
    case kOpRead: {
        if (n < 6) { return false; }
        size_t plen = n - 5;
        if (plen >= kPathMax) { status = kBadPath; return false; }
        out.offset = getU32(p + 1);
        memcpy(out.path, p + 5, plen);
        out.path[plen] = '\0';
        if (strlen(out.path) != plen || !validPath(out.path)) { status = kBadPath; return false; }
        break;
    }
    default:
        return false;
    }
    status = kOk;
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
size_t maxPacket(uint16_t mtu)
{
    size_t m = (mtu < 23) ? 20 : (size_t)mtu - 3;
    return m > kMaxPacket ? kMaxPacket : m;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void putU16(uint8_t* p, uint16_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void putU32(uint8_t* p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) { p[i] = (uint8_t)(v >> (8 * i)); }
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
uint32_t getU32(const uint8_t* p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
size_t encodeData(uint32_t offset, const uint8_t* data, size_t n, uint8_t* out)
{
    out[0] = kOpData;
    putU32(out + 1, offset);
    memcpy(out + kDataHeader, data, n);
    return kDataHeader + n;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
size_t encodeReadEnd(Status st, uint32_t size, uint32_t crc, uint8_t* out)
{
    out[0] = kOpReadEnd;
    out[1] = st;
    putU32(out + 2, size);
    putU32(out + 6, crc);
    return kReadEndSize;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
size_t encodeError(Status st, uint8_t* out)
{
    out[0] = kOpError;
    out[1] = st;
    return 2;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
size_t encodeLive(const Sample& s, uint8_t* out)
{
    putU32(out, s.utc);
    putU16(out + 4, s.co2);
    putU16(out + 6, (uint16_t)s.temp_c100);
    putU16(out + 8, s.rh_c100);
    uint32_t lux;
    memcpy(&lux, &s.lux, 4);
    putU32(out + 10, lux);
    out[14] = s.mode;
    out[15] = s.flags;
    return kLiveSize;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool decodeTimeMs(const uint8_t* p, size_t n, int64_t& ms)
{
    if (n != 8) { return false; }
    uint64_t v = (uint64_t)getU32(p) | ((uint64_t)getU32(p + 4) << 32);
    ms = (int64_t)v;
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void ListPacker::begin(size_t max_packet)
{
    max_ = max_packet > kMaxPacket ? kMaxPacket : max_packet;
    seq_ = 0;
    count_ = 0;
    crc_ = 0;
    buf_[0] = kOpListLines;
    putU16(buf_ + 1, seq_);
    len_ = 3;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool ListPacker::add(const char* path, uint32_t size)
{
    char line[kPathMax + 16];
    int n = snprintf(line, sizeof(line), "%s,%lu\n", path, (unsigned long)size);
    if (n <= 0 || (size_t)n >= sizeof(line)) { return true; }   // cannot happen for valid paths: skip
    if (len_ + (size_t)n > max_) { return false; }
    memcpy(buf_ + len_, line, (size_t)n);
    len_ += (size_t)n;
    crc_ = crc32Update(crc_, (const uint8_t*)line, (size_t)n);
    ++count_;
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void ListPacker::next()
{
    ++seq_;
    putU16(buf_ + 1, seq_);
    len_ = 3;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
size_t ListPacker::encodeEnd(uint8_t* out) const
{
    out[0] = kOpListEnd;
    putU16(out + 1, count_);
    putU32(out + 3, crc_);
    return kListEndSize;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void ReadWindow::begin(uint32_t offset, uint32_t size, uint32_t window)
{
    size_ = size;
    next_ = offset > size ? size : offset;
    acked_ = next_;
    window_ = window;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
uint32_t ReadWindow::allowed() const
{
    uint32_t limit = acked_ + window_;
    if (limit > size_) { limit = size_; }
    return limit > next_ ? limit - next_ : 0;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void ReadWindow::sent(uint32_t n)
{
    next_ += n;
    if (next_ > size_) { next_ = size_; }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void ReadWindow::ack(uint32_t offset)
{
    if (offset > next_) { offset = next_; }
    if (offset > acked_) { acked_ = offset; }
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
size_t formatStatus(const StatusInfo& s, char* buf, size_t len)
{
    int n = snprintf(buf, len,
                     "{\"fw\":\"%s\",\"utc_ms\":%lld,\"clock_valid\":%d,\"last_sync\":%lld,\"bat_pct\":%d,"
                     "\"bat_mv\":%d,\"usb\":%d,\"mode\":\"%s\",\"eff_mode\":\"%s\",\"sd\":\"%s\",\"free_kb\":%lu,"
                     "\"run_no\":%d,\"run_dir\":\"%s\",\"run_start\":%lld,\"last_cal\":%lld,\"window_s\":%d,"
                     "\"live_fast\":%d}",
                     s.fw, (long long)s.utc_ms, (int)s.clock_valid, (long long)s.last_sync, s.bat_pct, s.bat_mv,
                     (int)s.usb, modeName(s.mode), modeName(s.eff_mode), s.sd, (unsigned long)s.free_kb, s.run_no,
                     s.run_dir, (long long)s.run_start, (long long)s.last_cal, s.window_s, (int)s.live_fast);
    if (n < 0) { return 0; }
    return ((size_t)n < len) ? (size_t)n : len - 1;
}

} // namespace bleproto
} // namespace kf
