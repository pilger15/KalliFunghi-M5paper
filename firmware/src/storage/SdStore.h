/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// SD card: mount, batched CSV logging, config.json, history for long plot ranges.
//
// SPI sharing: SD and IT8951 share SPI3 (SCK14 MOSI12 MISO13). Every method here
// must only be called while the EPD is powered and no refresh is being pushed
// (Display::busPowered()). With GPIO23 low the unpowered IT8951 loads MISO.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ColumnBins.h"
#include "SampleRing.h"
#include "Types.h"
#include "config.h"

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

    Sample buf_[kCapacity];
    size_t head_ = 0;    // oldest
    size_t count_ = 0;
    bool mounted_ = false;
    bool full_ = false;
    bool write_failed_ = false;
    uint32_t dropped_ = 0;
};

} // namespace kf
