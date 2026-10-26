/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "SdStore.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <string.h>
#include "CsvFormat.h"
#include "hw/DebugTrace.h"

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool SdStore::mount()
{
    SD.end();
    mounted_ = SD.begin(kPinSdCs, SPI, kSdSpiHz);
    write_failed_ = false;
    if (mounted_) {
        if (!SD.exists("/log")) { SD.mkdir("/log"); }
        checkFree();
    }
    KF_TRACE("sd mount %d", (int)mounted_);
    return mounted_;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void SdStore::checkFree()
{
    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    full_ = total > used ? (total - used) < kSdMinFreeBytes : true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// A pulled and re-inserted card needs a fresh SD.begin; cardType() is cheap.
bool SdStore::remountIfNeeded()
{
    if (mounted_ && !write_failed_ && SD.cardType() != CARD_NONE) { return true; }
    return mount();
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void SdStore::queue(const Sample& s)
{
    if (count_ == kCapacity) {
        head_ = (head_ + 1) % kCapacity;
        --count_;
        ++dropped_;
    }
    buf_[(head_ + count_) % kCapacity] = s;
    ++count_;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool SdStore::writeRows(const char* path, size_t first, size_t n)
{
    File f = SD.open(path, FILE_APPEND);
    if (!f) { return false; }
    bool ok = true;
    if (f.size() == 0) { ok = f.println(kCsvHeader) > 0; }
    char row[kCsvRowMax];
    for (size_t i = 0; ok && i < n; ++i) {
        size_t len = formatRow(buf_[(head_ + first + i) % kCapacity], row, sizeof(row));
        ok = f.write((const uint8_t*)row, len) == len && f.write('\n') == 1;
    }
    f.close();
    return ok;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Writes rows grouped by target file (UTC day, or unsynced.csv), so a batch that
// spans midnight goes into two files.
bool SdStore::flush()
{
    if (count_ == 0) { return true; }
    if (!remountIfNeeded()) { return false; }
    checkFree();
    if (full_) { return false; }

    while (count_ > 0) {
        const Sample& first = buf_[head_];
        bool unsynced = first.flags & Sample::kUnsynced;
        int64_t day = utcDay(first.utc);
        size_t n = 1;
        while (n < count_) {
            const Sample& s = buf_[(head_ + n) % kCapacity];
            bool su = s.flags & Sample::kUnsynced;
            if (su != unsynced || (!su && utcDay(s.utc) != day)) { break; }
            ++n;
        }
        char path[32];
        if (unsynced) {
            strncpy(path, kUnsyncedLogPath, sizeof(path));
            path[sizeof(path) - 1] = '\0';
        } else {
            dayLogPath(first.utc, path, sizeof(path));
        }
        if (!writeRows(path, 0, n)) {
            write_failed_ = true;
            KF_TRACE("sd write failed %s", path);
            return false;
        }
        head_ = (head_ + n) % kCapacity;
        count_ -= n;
    }
    write_failed_ = false;
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void SdStore::logSync(int64_t utc, int64_t correction_s)
{
    if (!remountIfNeeded()) { return; }
    File f = SD.open(kSyncLogPath, FILE_APPEND);
    if (!f) { return; }
    if (f.size() == 0) { f.println("utc_iso,correction_s"); }
    char iso[24];
    formatIsoUtc(utc, iso, sizeof(iso));
    f.printf("%s,%lld\n", iso, (long long)correction_s);
    f.close();
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void SdStore::loadConfig(WifiConfig& wifi)
{
    if (!mounted_) { return; }
    File f = SD.open("/config.json", FILE_READ);
    if (!f) { return; }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        KF_TRACE("config.json: %s", err.c_str());
        return;
    }
    const char* ssid = doc["wifi"]["ssid"] | (const char*)nullptr;
    const char* pass = doc["wifi"]["password"] | (const char*)nullptr;
    if (ssid && *ssid) {
        strncpy(wifi.ssid, ssid, sizeof(wifi.ssid) - 1);
        wifi.ssid[sizeof(wifi.ssid) - 1] = '\0';
    }
    if (pass) {
        strncpy(wifi.password, pass, sizeof(wifi.password) - 1);
        wifi.password[sizeof(wifi.password) - 1] = '\0';
    }
}

namespace {

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Calls fn(sample) for every synced row with t0 <= utc < t1, day file by day file.
template <typename Fn>
void forEachRow(int64_t t0, int64_t t1, Fn fn)
{
    char line[kCsvRowMax + 8];
    for (int64_t day = utcDay(t0); day <= utcDay(t1 - 1); ++day) {
        char path[32];
        dayLogPath(day * 86400, path, sizeof(path));
        File f = SD.open(path, FILE_READ);
        if (!f) { continue; }
        while (f.available()) {
            size_t n = f.readBytesUntil('\n', line, sizeof(line) - 1);
            line[n] = '\0';
            Sample s;
            if (!parseRow(line, s) || (s.flags & Sample::kUnsynced)) { continue; }
            if ((int64_t)s.utc < t0 || (int64_t)s.utc >= t1) { continue; }
            fn(s);
        }
        f.close();
    }
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int64_t SdStore::loadHistory(ColumnBins* bins[kFieldCount], int64_t t0, int64_t t1)
{
    int64_t newest = 0;
    if (!remountIfNeeded()) { return 0; }
    forEachRow(t0, t1, [&](const Sample& s) {
        for (int f = 0; f < kFieldCount; ++f) {
            float v;
            if (sampleValue(s, (PlotField)f, v)) { bins[f]->add(s.utc, v); }
        }
        if ((int64_t)s.utc > newest) { newest = s.utc; }
    });
    return newest;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void SdStore::loadRing(SampleRing& ring, int64_t now_utc, uint32_t seconds)
{
    if (!mounted_) { return; }
    forEachRow(now_utc - seconds, now_utc + 1, [&](const Sample& s) { ring.push(s); });
}

} // namespace kf
