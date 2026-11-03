/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// BLE sync protocol: the byte formats of the GATT characteristics (README "Bluetooth").
// Pure, unit tested; the radio side is src/ble/BleLink.
//
// Files characteristic (write a request, answers come as notifications):
//   LIST  01                          -> 81 seq:u16 "path,size\n"...   (several packets)
//                                        82 count:u16 crc32:u32        (CRC over all line bytes)
//   READ  02 offset:u32 path          -> 83 offset:u32 data...         (several packets)
//                                        84 status:u8 size:u32 crc32:u32
//   ACK   03 offset:u32               flow control: the device sends at most kWindowBytes
//                                     past the last acknowledged offset
//   ABORT 04                          stops a running READ (84 with kAborted)
//   error                             -> 8F code:u8
// Paths are relative to /log: "error.log", "sync.csv" or "<run folder>/<file>".
// The READ CRC covers the bytes [offset, size) of this transfer, so for offset 0 it is
// the CRC of the whole file. All integers little endian.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "Types.h"

namespace kf {
namespace bleproto {

enum Op : uint8_t {
    kOpList = 0x01,
    kOpRead = 0x02,
    kOpAck = 0x03,
    kOpAbort = 0x04,
    kOpListLines = 0x81,
    kOpListEnd = 0x82,
    kOpData = 0x83,
    kOpReadEnd = 0x84,
    kOpError = 0x8F,
};

enum Status : uint8_t {
    kOk = 0,
    kBadRequest = 1,
    kBadPath = 2,
    kNotFound = 3,
    kBusy = 4,
    kSdError = 5,
    kAborted = 6,
};

// Control characteristic (write without response).
enum ControlOp : uint8_t {
    kCtlKeepalive = 0x01,   // page activity, restarts the device idle timeout
    kCtlFast = 0x02,        // 02 on:u8, Fast sampling while connected
    kCtlEnd = 0x03,         // end the session: disconnect, radio off
};

constexpr size_t kPathMax = 40;           // relative path incl. NUL ("m004_2026-11-03/2026-11-03.csv" is 30)
constexpr uint32_t kWindowBytes = 8192;   // unacknowledged bytes in flight
constexpr size_t kDataHeader = 5;         // 83 offset:u32
constexpr size_t kMaxPacket = 514;        // notification payload at MTU 517
constexpr size_t kLiveSize = 16;
constexpr size_t kReadEndSize = 10;
constexpr size_t kListEndSize = 7;

// CRC-32 (IEEE 802.3, as zlib). Start with crc = 0 and feed the bytes in any split.
uint32_t crc32Update(uint32_t crc, const uint8_t* p, size_t n);

// True for "error.log", "sync.csv" and "<run folder>/<name>" (folder per parseRunNumber,
// name without '/', '\\', leading '.' or "..").
bool validPath(const char* rel);

struct Request {
    Op op = kOpList;
    uint32_t offset = 0;      // READ, ACK
    char path[kPathMax] = ""; // READ
};
// Parses one write to the Files characteristic. false: malformed or a bad path
// (status tells which, kBadRequest or kBadPath).
bool parseRequest(const uint8_t* p, size_t n, Request& out, Status& status);

// Notification payload size for an ATT MTU (3 bytes ATT header), capped at kMaxPacket.
size_t maxPacket(uint16_t mtu);
// Data bytes per READ packet at this MTU.
inline size_t dataPayload(uint16_t mtu) { return maxPacket(mtu) - kDataHeader; }

void putU16(uint8_t* p, uint16_t v);
void putU32(uint8_t* p, uint32_t v);
uint32_t getU32(const uint8_t* p);

// 83 offset:u32 + data. out needs kDataHeader + n bytes. Returns the length.
size_t encodeData(uint32_t offset, const uint8_t* data, size_t n, uint8_t* out);
// 84 status size crc. out needs kReadEndSize bytes.
size_t encodeReadEnd(Status st, uint32_t size, uint32_t crc, uint8_t* out);
// 8F code.
size_t encodeError(Status st, uint8_t* out);

// Live sample: utc u32, co2 u16, temp_c100 i16, rh_c100 u16, lux f32, mode u8, flags u8.
size_t encodeLive(const Sample& s, uint8_t* out);
// Time characteristic: int64 UTC milliseconds. false if not 8 bytes.
bool decodeTimeMs(const uint8_t* p, size_t n, int64_t& ms);

// Packs "path,size\n" lines into LIST packets of at most max_packet bytes.
class ListPacker {
public:
    void begin(size_t max_packet);
    // Appends a line. false if it does not fit: send packet(), call next(), add again.
    bool add(const char* path, uint32_t size);
    const uint8_t* packet() const { return buf_; }
    size_t size() const { return len_; }
    bool hasLines() const { return len_ > 3; }
    // Starts the next packet (sequence + 1).
    void next();
    uint16_t count() const { return count_; }
    // 82 count crc. out needs kListEndSize bytes.
    size_t encodeEnd(uint8_t* out) const;

private:
    uint8_t buf_[kMaxPacket];
    size_t max_ = kMaxPacket;
    size_t len_ = 0;
    uint16_t seq_ = 0;
    uint16_t count_ = 0;
    uint32_t crc_ = 0;
};

// Credit window of one READ. Offsets are file offsets.
class ReadWindow {
public:
    void begin(uint32_t offset, uint32_t size, uint32_t window = kWindowBytes);
    // Bytes that may be sent now, starting at next().
    uint32_t allowed() const;
    uint32_t next() const { return next_; }
    uint32_t size() const { return size_; }
    void sent(uint32_t n);
    // An ACK beyond what was sent is clamped; an older one is ignored.
    void ack(uint32_t offset);
    bool done() const { return next_ >= size_; }

private:
    uint32_t next_ = 0;
    uint32_t acked_ = 0;
    uint32_t size_ = 0;
    uint32_t window_ = kWindowBytes;
};

// Status characteristic as JSON.
struct StatusInfo {
    const char* fw = "";
    int64_t utc_ms = 0;
    bool clock_valid = false;
    int64_t last_sync = 0;    // UTC s, 0 = never
    int bat_pct = 0;
    int bat_mv = 0;
    bool usb = false;
    SamplingMode mode = SamplingMode::Eco;      // user setting
    SamplingMode eff_mode = SamplingMode::Eco;  // what runs now
    const char* sd = "ok";    // ok, missing, full, error
    uint32_t free_kb = 0;
    int run_no = 0;
    const char* run_dir = "";
    int64_t run_start = 0;
    int64_t last_cal = 0;     // 0 = never
    int window_s = 0;         // seconds until the BLE window ends (0 = held by the connection)
    bool live_fast = false;
};
size_t formatStatus(const StatusInfo& s, char* buf, size_t len);

} // namespace bleproto
} // namespace kf
