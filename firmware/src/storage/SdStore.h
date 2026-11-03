/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// SD card: mount, batched CSV logging, config.json, history for long plot ranges.
//
// SPI sharing: SD and IT8951 share SPI3 (SCK14 MOSI12 MISO13). Every method here
// must only be called while no refresh is being pushed. The EPD stays powered
// (Display keeps GPIO23 high), so its MISO load is not an issue.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ColumnBins.h"
#include "SampleRing.h"
#include "Types.h"
#include "config.h"
#include "hw/DebugTrace.h"

namespace kf {

struct WifiConfig {
    char ssid[33];
    char password[65];
};

class SdStore {
public:
    bool mount();
    bool mounted() const { return mounted_; }
    bool full() const { return full_; }
    // Problem mark for the sidebar: missing, unmountable, full, or write failed.
    bool problem() const { return !mounted_ || full_ || write_failed_; }
    uint32_t droppedRows() const { return dropped_; }

    // Queues one row in RAM (no SD access). Drops the oldest row if the buffer is full.
    void queue(const Sample& s);
    size_t queued() const { return count_; }
    bool bufferNearlyFull() const { return count_ >= kCapacity - 8; }
    // Writes all queued rows. Keeps them on failure. Returns true if the queue is empty after.
    bool flush();
    // Appends a line to /log/sync.csv: utc_iso,correction_s.
    void logSync(int64_t utc, int64_t correction_s);

    // Error log /log/error.log (utc_iso,clock,text). event() only queues in RAM, so it is
    // safe at any time (also with no card); the lines are written by the next flush()
    // or flushEvents(), i.e. as soon as a working card is present. Keeps the newest 16.
    // The caller refreshes the timestamp source once per tick and flushes early when
    // eventsNearlyFull(), so a burst cannot push older lines (e.g. the boot line) out.
    /* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
    void setTime(int64_t utc, bool clock_valid) { now_utc_ = utc; now_valid_ = clock_valid; }
    void event(const char* fmt, ...) __attribute__((format(printf, 2, 3)));
    size_t pendingEvents() const { return ev_count_; }
    bool eventsNearlyFull() const { return ev_count_ >= kEventSlots - 4; }
    bool flushEvents();

    // Fills wifi from /config.json if present ({"wifi":{"ssid":..,"password":..}}).
    // Leaves it unchanged otherwise.
    void loadConfig(WifiConfig& wifi);

    // Streams the daily files covering [t0, t1) into the four field bins.
    // Returns the newest UTC found on SD (0 if none).
    int64_t loadHistory(ColumnBins* bins[kFieldCount], int64_t t0, int64_t t1);
    // Fills the ring with the last seconds of synced rows (after a reboot).
    void loadRing(SampleRing& ring, int64_t now_utc, uint32_t seconds);

private:
    static constexpr size_t kCapacity = kLogBufferRows;
    bool writeRows(const char* path, size_t first, size_t n);
    bool remountIfNeeded();
    void checkFree();
#if KF_TRACE_ENABLED
    void traceCard();
#endif

    static constexpr size_t kEventSlots = 16;
    static constexpr size_t kEventLen = 112;

    Sample buf_[kCapacity];
    size_t head_ = 0;    // oldest
    size_t count_ = 0;
    bool mounted_ = false;
    bool full_ = false;
    bool write_failed_ = false;
    uint32_t dropped_ = 0;
    uint32_t dropped_reported_ = 0;
    bool fail_reported_ = false;   // a write failure is in the log already
    bool full_reported_ = false;

    int64_t now_utc_ = 0;
    bool now_valid_ = false;
    char ev_[kEventSlots][kEventLen];
    size_t ev_head_ = 0;
    size_t ev_count_ = 0;
};

} // namespace kf
