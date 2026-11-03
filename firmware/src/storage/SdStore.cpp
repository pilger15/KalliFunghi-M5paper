/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "SdStore.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include "CsvFormat.h"
#include "RunLog.h"
#include "config.h"
#include "hw/DebugTrace.h"

namespace kf {

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Also recreates the run folder and run.txt on a new or swapped card.
bool SdStore::mount()
{
    bool was_mounted = mounted_;
    SD.end();
    mounted_ = SD.begin(kPinSdCs, SPI, kSdSpiHz);
    write_failed_ = false;
    if (mounted_) {
        if (!SD.exists(kLogDir)) { SD.mkdir(kLogDir); }
        checkFree();
        if (hasRun()) { ensureRunDir(); }
        if (!was_mounted) { event("sd mounted, type %d", (int)SD.cardType()); }
    } else if (was_mounted) {
        event("sd mount failed (card removed or unreadable)");
    }
    KF_TRACE("sd mount %d", (int)mounted_);
#if KF_TRACE_ENABLED
    if (mounted_) { traceCard(); }
#endif
    return mounted_;
}

#if KF_TRACE_ENABLED
/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// Debug builds only: the current run folder and its run.txt.
void SdStore::traceRun()
{
    if (!mounted_ || !hasRun()) { return; }
    char path[kLogPathMax];
    snprintf(path, sizeof(path), "%s/%s", kLogDir, run_.dir);
    File rd = SD.open(path);
    if (rd && rd.isDirectory()) {
        for (File f = rd.openNextFile(); f; f = rd.openNextFile()) {
            KF_TRACE("sd   %s/%s %u", run_.dir, f.name(), (unsigned)f.size());
            f.close();
        }
    }
    if (rd) { rd.close(); }
    runFilePath(run_.dir, kRunInfoName, path, sizeof(path));
    File f = SD.open(path, FILE_READ);
    if (!f) { return; }
    char line[kEventLen + 16];
    while (f.available()) {
        size_t len = f.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        KF_TRACE("run.txt %s", line);
    }
    f.close();
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
// Debug builds only: card type, size and the /log listing, to see the card state without
// pulling it.
void SdStore::traceCard()
{
    KF_TRACE("sd type %d, total %llu, used %llu", (int)SD.cardType(), (unsigned long long)SD.totalBytes(),
             (unsigned long long)SD.usedBytes());
    File dir = SD.open("/log");
    if (!dir || !dir.isDirectory()) {
        KF_TRACE("sd /log missing");
        return;
    }
    int n = 0;
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        KF_TRACE("sd   %s %u", f.name(), (unsigned)f.size());
        f.close();
        ++n;
    }
    dir.close();
    KF_TRACE("sd /log has %d entries", n);
    traceRun();
    // Tail of the error log: what happened in earlier runs.
    File e = SD.open(kErrorLogPath, FILE_READ);
    if (!e) { return; }
    if (e.size() > 3000) { e.seek(e.size() - 3000); }
    char line[kEventLen + 16];
    while (e.available()) {
        size_t len = e.readBytesUntil('\n', line, sizeof(line) - 1);
        line[len] = '\0';
        KF_TRACE("errlog %s", line);
    }
    e.close();
}
#endif

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// A total of 0 means the FAT query failed, not that the card is full: keep logging.
void SdStore::checkFree()
{
    uint64_t total = SD.totalBytes();
    uint64_t used = SD.usedBytes();
    if (total == 0) {
        full_ = false;
        event("sd size query failed, assuming space left");
        return;
    }
    free_ = total > used ? total - used : 0;
    full_ = total > used ? (total - used) < kSdMinFreeBytes : true;
    if (full_ && !full_reported_) { event("sd full, %llu bytes free", (unsigned long long)(total > used ? total - used : 0)); }
    full_reported_ = full_;
}

/* Created by Claude Sonnet 5.5 | Review priority: Medium | Reviewed by: None */
void SdStore::event(const char* fmt, ...)
{
    if (ev_count_ == kEventSlots) {
        ev_head_ = (ev_head_ + 1) % kEventSlots;
        --ev_count_;
    }
    size_t slot = (ev_head_ + ev_count_) % kEventSlots;
    char iso[24];
    formatIsoUtc(now_utc_, iso, sizeof(iso));
    int n = snprintf(ev_[slot], kEventLen, "%s,%s,", iso, now_valid_ ? "synced" : "unsynced");
    va_list ap;
    va_start(ap, fmt);
    if (n > 0 && (size_t)n < kEventLen) { vsnprintf(ev_[slot] + n, kEventLen - n, fmt, ap); }
    va_end(ap);
    ++ev_count_;
    KF_TRACE("event %s", ev_[slot]);
}

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Keeps the lines queued when the card is not usable, so the log shows up once it is.
// Also writes the queued run.txt lines.
bool SdStore::flushEvents()
{
    if (ev_count_ == 0) { return flushRunInfo(); }
    if (!mounted_ || write_failed_) { return false; }
    File f = SD.open(kErrorLogPath, FILE_APPEND);
    if (!f) { return false; }
    bool ok = true;
    if (f.size() == 0) { ok = f.println("utc_iso,clock,event") > 0; }
    for (size_t i = 0; ok && i < ev_count_; ++i) {
        const char* line = ev_[(ev_head_ + i) % kEventSlots];
        size_t len = strlen(line);
        ok = f.write((const uint8_t*)line, len) == len && f.write('\n') == 1;
    }
    f.close();
    if (ok) {
        ev_head_ = 0;
        ev_count_ = 0;
    }
    return flushRunInfo() && ok;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void SdStore::runInfo(const char* fmt, ...)
{
    if (info_count_ == kInfoSlots) {
        // Rare (card missing for a long time): keep the oldest lines, drop the newest.
        event("run.txt line dropped, queue full");
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(info_[info_count_], kEventLen, fmt, ap);
    va_end(ap);
    ++info_count_;
    KF_TRACE("run info %s", info_[info_count_ - 1]);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool SdStore::flushRunInfo()
{
    if (info_count_ == 0) { return true; }
    if (!mounted_ || write_failed_ || !hasRun()) { return false; }
    char path[kLogPathMax];
    runFilePath(run_.dir, kRunInfoName, path, sizeof(path));
    File f = SD.open(path, FILE_APPEND);
    if (!f) { return false; }
    bool ok = true;
    for (size_t i = 0; ok && i < info_count_; ++i) {
        size_t len = strlen(info_[i]);
        ok = f.write((const uint8_t*)info_[i], len) == len && f.write('\n') == 1;
    }
    f.close();
    if (ok) { info_count_ = 0; }
    return ok;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void SdStore::setRun(const RunInfo& run)
{
    run_ = run;
    if (mounted_ && hasRun()) { ensureRunDir(); }
#if KF_TRACE_ENABLED
    traceRun();
#endif
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// run.txt from run_, plus an optional extra line. Overwrites an existing file.
bool SdStore::writeRunInfoFile(const char* extra_line)
{
    char path[kLogPathMax];
    runFilePath(run_.dir, kRunInfoName, path, sizeof(path));
    File f = SD.open(path, FILE_WRITE);
    if (!f) { return false; }
    char body[320];
    size_t len = formatRunInfo(run_, kFirmwareVersion, body, sizeof(body));
    bool ok = f.write((const uint8_t*)body, len) == len;
    if (ok && extra_line) { ok = f.println(extra_line) > 0; }
    f.close();
    return ok;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Folder and run.txt of the current run, if missing (new or swapped card). The recreated
// run.txt says so: its values are the ones kept in NVS, not read from the old card.
bool SdStore::ensureRunDir()
{
    char path[kLogPathMax];
    snprintf(path, sizeof(path), "%s/%s", kLogDir, run_.dir);
    if (!SD.exists(path) && !SD.mkdir(path)) {
        event("cannot create run folder %s", run_.dir);
        return false;
    }
    runFilePath(run_.dir, kRunInfoName, path, sizeof(path));
    if (SD.exists(path)) { return true; }
    char iso[24];
    formatIsoUtc(now_utc_, iso, sizeof(iso));
    char extra[48];
    snprintf(extra, sizeof(extra), "recreated_utc=%s", iso);
    bool ok = writeRunInfoFile(extra);
    event("run folder %s %s on this card", run_.dir, ok ? "recreated" : "could not be recreated");
    return ok;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int SdStore::maxRunOnCard()
{
    int max_run = 0;
    File dir = SD.open(kLogDir);
    if (!dir || !dir.isDirectory()) { return 0; }
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        if (f.isDirectory()) {
            int n = parseRunNumber(f.name());
            if (n > max_run) { max_run = n; }
        }
        f.close();
    }
    dir.close();
    return max_run;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Pending run.txt lines belong to the old run, so they are written before the switch
// (or dropped if that fails: the error log has the same events).
bool SdStore::startRun(const RunInfo& run)
{
    if (!ready()) { return false; }
    flushRunInfo();
    info_count_ = 0;
    char path[kLogPathMax];
    snprintf(path, sizeof(path), "%s/%s", kLogDir, run.dir);
    if (!SD.exists(path) && !SD.mkdir(path)) {
        event("cannot create run folder %s", run.dir);
        return false;
    }
    RunInfo old = run_;
    run_ = run;
    if (!writeRunInfoFile(nullptr)) {
        event("cannot write %s/%s", run.dir, kRunInfoName);
        run_ = old;
        return false;
    }
#if KF_TRACE_ENABLED
    traceRun();
#endif
    return true;
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

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool SdStore::writeRows(const char* path, size_t first, size_t n)
{
    File f = SD.open(path, FILE_APPEND);
    KF_TRACE("sd open %s %d, %u rows", path, (int)(bool)f, (unsigned)n);
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

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Writes rows grouped by target file (UTC day, or unsynced.csv, in the run folder), so a
// batch that spans midnight goes into two files. Then writes the queued error-log lines.
// The card is only probed by writing: after a pull, the first flush fails (rows stay
// queued), the next one remounts.
bool SdStore::flush()
{
    if (count_ == 0 && ev_count_ == 0) { return true; }
    if (!remountIfNeeded()) { return false; }
    if (count_ == 0) { return flushEvents(); }
    if (!hasRun()) {
        // First boot with no usable card yet: the App starts run 1 once it can.
        flushEvents();
        return false;
    }
    checkFree();
    if (full_) {
        flushEvents();
        return false;
    }

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
        char path[kLogPathMax];
        if (unsynced) {
            runFilePath(run_.dir, kUnsyncedName, path, sizeof(path));
        } else {
            runDayLogPath(run_.dir, first.utc, path, sizeof(path));
        }
        if (!writeRows(path, 0, n)) {
            write_failed_ = true;
            if (!fail_reported_) {
                event("sd write failed on %s, %u rows queued", path, (unsigned)count_);
                fail_reported_ = true;
            }
            KF_TRACE("sd write failed %s", path);
            return false;
        }
        head_ = (head_ + n) % kCapacity;
        count_ -= n;
    }
    write_failed_ = false;
    if (fail_reported_) {
        event("sd write ok again");
        fail_reported_ = false;
    }
    if (dropped_ != dropped_reported_) {
        event("%lu rows lost (buffer overflow while the card was unavailable)",
              (unsigned long)(dropped_ - dropped_reported_));
        dropped_reported_ = dropped_;
    }
    flushEvents();
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

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Calls fn(sample) for every synced row with t0 <= utc < t1, day file by day file of
// one run folder.
template <typename Fn>
void forEachRow(const char* run_dir, int64_t t0, int64_t t1, Fn fn)
{
    char line[kCsvRowMax + 8];
    for (int64_t day = utcDay(t0); day <= utcDay(t1 - 1); ++day) {
        char path[kLogPathMax];
        runDayLogPath(run_dir, day * 86400, path, sizeof(path));
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

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
int64_t SdStore::loadHistory(ColumnBins* bins[kFieldCount], int64_t t0, int64_t t1)
{
    int64_t newest = 0;
    if (!hasRun() || !remountIfNeeded()) { return 0; }
    forEachRow(run_.dir, t0, t1, [&](const Sample& s) {
        for (int f = 0; f < kFieldCount; ++f) {
            float v;
            if (sampleValue(s, (PlotField)f, v)) { bins[f]->add(s.utc, v); }
        }
        if ((int64_t)s.utc > newest) { newest = s.utc; }
    });
    return newest;
}

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void SdStore::loadRing(SampleRing& ring, int64_t now_utc, uint32_t seconds)
{
    if (!mounted_ || !hasRun()) { return; }
    forEachRow(run_.dir, now_utc - seconds, now_utc + 1, [&](const Sample& s) { ring.push(s); });
}

} // namespace kf

namespace kf {

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// error.log and sync.csv first, then every run folder's files. Old flat files and other
// folders are not listed (BleProto::validPath would refuse them anyway).
bool SdStore::listFiles(const std::function<void(const char* rel, uint32_t size)>& fn)
{
    if (!remountIfNeeded()) { return false; }
    File dir = SD.open(kLogDir);
    if (!dir || !dir.isDirectory()) { return false; }
    char rel[bleproto::kPathMax];
    for (File f = dir.openNextFile(); f; f = dir.openNextFile()) {
        const char* name = f.name();
        const char* slash = strrchr(name, '/');
        if (slash) { name = slash + 1; }
        if (!f.isDirectory()) {
            if (strcmp(name, "error.log") == 0 || strcmp(name, "sync.csv") == 0) { fn(name, (uint32_t)f.size()); }
        } else if (parseRunNumber(name) >= 0) {
            char dir_name[kRunDirMax];
            snprintf(dir_name, sizeof(dir_name), "%s", name);
            for (File g = f.openNextFile(); g; g = f.openNextFile()) {
                const char* gn = g.name();
                const char* gs = strrchr(gn, '/');
                if (gs) { gn = gs + 1; }
                snprintf(rel, sizeof(rel), "%s/%s", dir_name, gn);
                if (!g.isDirectory() && bleproto::validPath(rel)) { fn(rel, (uint32_t)g.size()); }
                g.close();
            }
        }
        f.close();
    }
    dir.close();
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Opens, seeks, reads and closes again, so no file stays open across a flush() that may
// append to the same day file.
int SdStore::readChunk(const char* rel, uint32_t offset, uint8_t* buf, size_t len, uint32_t& size)
{
    if (!remountIfNeeded()) { return -1; }
    char path[kLogPathMax + 8];
    snprintf(path, sizeof(path), "%s/%s", kLogDir, rel);
    File f = SD.open(path, FILE_READ);
    if (!f || f.isDirectory()) { return -2; }
    size = (uint32_t)f.size();
    int n = 0;
    if (len > 0 && offset < size) {
        if (!f.seek(offset)) {
            f.close();
            return -1;
        }
        n = (int)f.read(buf, len);
    }
    f.close();
    return n;
}

} // namespace kf
