/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
#include "BleSync.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <stdio.h>
#include <string.h>
#include "RunLog.h"
#include "config.h"
#include "hw/DebugTrace.h"
#include "storage/SdStore.h"

namespace kf {

using namespace bleproto;

namespace {

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
inline int32_t diff(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

enum class Ev : uint8_t { Connect, Disconnect, Files, Time, Control };

struct Event {
    Ev type;
    uint8_t len;
    uint32_t ms;
    uint8_t data[kPathMax + 8];
};

constexpr int kQueueDepth = 16;

// Shared with the NimBLE host task.
QueueHandle_t g_queue = nullptr;
NimBLEServer* g_server = nullptr;
NimBLECharacteristic* g_live = nullptr;
NimBLECharacteristic* g_files = nullptr;
std::atomic<uint16_t> g_conn{BLE_HS_CONN_HANDLE_NONE};
std::atomic<uint16_t> g_mtu{23};
std::atomic<bool> g_live_sub{false};

// Status snapshot, written by the App loop, read in the host task.
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
StatusInfo g_info;
char g_run_dir[kRunDirMax] = "";
const char* g_sd = "ok";
int64_t g_utc = 0;
uint32_t g_utc_ms = 0;

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void push(Ev type, const uint8_t* p, size_t n)
{
    if (!g_queue) { return; }
    Event e;
    e.type = type;
    e.ms = millis();
    if (n > sizeof(e.data)) { n = sizeof(e.data); }   // too long: parseRequest rejects the cut copy
    e.len = (uint8_t)n;
    if (n) { memcpy(e.data, p, n); }
    xQueueSend(g_queue, &e, 0);   // full queue: dropped; the page times out and retries
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* s, NimBLEConnInfo& ci) override
    {
        g_conn = ci.getConnHandle();
        // 7.5..15 ms interval, no latency, 4 s supervision timeout.
        s->updateConnParams(ci.getConnHandle(), 6, 12, 0, 400);
        push(Ev::Connect, nullptr, 0);
    }
    void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int reason) override
    {
        g_conn = BLE_HS_CONN_HANDLE_NONE;
        g_live_sub = false;
        uint8_t r = (uint8_t)reason;
        push(Ev::Disconnect, &r, 1);
    }
    void onMTUChange(uint16_t mtu, NimBLEConnInfo&) override { g_mtu = mtu; }
};

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
class StatusCallbacks : public NimBLECharacteristicCallbacks {
    // Called at offset 0 of a (long) read only, so all parts come from one snapshot.
    void onRead(NimBLECharacteristic* c, NimBLEConnInfo&) override
    {
        StatusInfo s;
        char run_dir[kRunDirMax];
        int64_t utc;
        uint32_t ms;
        portENTER_CRITICAL(&g_mux);
        s = g_info;
        memcpy(run_dir, g_run_dir, sizeof(run_dir));
        s.sd = g_sd;
        utc = g_utc;
        ms = g_utc_ms;
        portEXIT_CRITICAL(&g_mux);
        s.run_dir = run_dir;
        s.utc_ms = utc * 1000 + (int64_t)(millis() - ms);
        char buf[320];
        size_t n = formatStatus(s, buf, sizeof(buf));
        c->setValue((const uint8_t*)buf, n);
    }
};

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
class WriteCallbacks : public NimBLECharacteristicCallbacks {
public:
    explicit WriteCallbacks(Ev type) : type_(type) {}
    void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override
    {
        NimBLEAttValue v = c->getValue();
        push(type_, v.data(), v.size());
    }
    void onSubscribe(NimBLECharacteristic* c, NimBLEConnInfo&, uint16_t sub) override
    {
        if (c == g_live) { g_live_sub = (sub & 1) != 0; }
    }

private:
    Ev type_;
};

ServerCallbacks g_server_cb;
StatusCallbacks g_status_cb;
WriteCallbacks g_time_cb(Ev::Time);
WriteCallbacks g_files_cb(Ev::Files);
WriteCallbacks g_control_cb(Ev::Control);
WriteCallbacks g_live_cb(Ev::Control);   // only onSubscribe is used (Live has no write)

} // namespace

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
bool BleSync::start(uint32_t now, uint32_t window_ms)
{
    if (on()) { return true; }
    if (!g_queue) { g_queue = xQueueCreate(kQueueDepth, sizeof(Event)); }
    if (!buf_) { buf_ = static_cast<uint8_t*>(malloc(kBufSize)); }
    if (!g_queue || !buf_) { return false; }
    xQueueReset(g_queue);
    g_conn = BLE_HS_CONN_HANDLE_NONE;
    g_mtu = 23;
    g_live_sub = false;

    if (!NimBLEDevice::init(kBleName)) {
        KF_TRACE("ble: init failed");
        return false;
    }
    NimBLEDevice::setMTU(517);
    g_server = NimBLEDevice::createServer();
    g_server->setCallbacks(&g_server_cb, false);
    g_server->advertiseOnDisconnect(false);   // one session per tap on the device

    NimBLEService* svc = g_server->createService(kBleServiceUuid);
    NimBLECharacteristic* status = svc->createCharacteristic(kBleStatusUuid, NIMBLE_PROPERTY::READ);
    status->setCallbacks(&g_status_cb);
    NimBLECharacteristic* time = svc->createCharacteristic(kBleTimeUuid, NIMBLE_PROPERTY::WRITE);
    time->setCallbacks(&g_time_cb);
    g_live = svc->createCharacteristic(kBleLiveUuid, NIMBLE_PROPERTY::NOTIFY);
    g_live->setCallbacks(&g_live_cb);
    g_files = svc->createCharacteristic(kBleFilesUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    g_files->setCallbacks(&g_files_cb);
    NimBLECharacteristic* control = svc->createCharacteristic(kBleControlUuid, NIMBLE_PROPERTY::WRITE_NR);
    control->setCallbacks(&g_control_cb);
    g_server->start();

    // 128-bit UUID and flags in the advertisement, the name in the scan response (31 B each).
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData ad;
    ad.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    ad.addServiceUUID(NimBLEUUID(kBleServiceUuid));
    NimBLEAdvertisementData sr;
    sr.setName(kBleName);
    adv->enableScanResponse(true);
    adv->setAdvertisementData(ad);
    adv->setScanResponseData(sr);
    if (!adv->start()) {
        KF_TRACE("ble: advertising failed");
        NimBLEDevice::deinit(true);
        g_server = nullptr;
        return false;
    }
    state_ = State::Advertising;
    end_ = End::None;
    changed_ = true;
    start_ms_ = now;
    window_ms_ = window_ms;
    activity_ms_ = now;
    fast_ = false;
    had_connection_ = false;
    time_pending_ = false;
    path_[0] = '\0';
    KF_TRACE("ble: advertising, heap %u", (unsigned)ESP.getFreeHeap());
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void BleSync::stop()
{
    if (!on()) { return; }
    if (g_server && g_conn != BLE_HS_CONN_HANDLE_NONE) { g_server->disconnect(g_conn); }
    NimBLEDevice::deinit(true);   // controller off, all GATT objects and stack RAM freed
    g_server = nullptr;
    g_live = nullptr;
    g_files = nullptr;
    g_conn = BLE_HS_CONN_HANDLE_NONE;
    g_live_sub = false;
    state_ = State::Off;
    fast_ = false;
    changed_ = true;
    free(buf_);
    buf_ = nullptr;
    KF_TRACE("ble: off, heap %u", (unsigned)ESP.getFreeHeap());
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
bool BleSync::takeChanged()
{
    bool c = changed_;
    changed_ = false;
    return c;
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
bool BleSync::takeTime(int64_t& utc_ms, uint32_t& rx_ms)
{
    if (!time_pending_) { return false; }
    time_pending_ = false;
    utc_ms = time_ms_;
    rx_ms = time_rx_ms_;
    return true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void BleSync::setStatus(const StatusInfo& s, int64_t utc, uint32_t utc_ms)
{
    portENTER_CRITICAL(&g_mux);
    g_info = s;
    snprintf(g_run_dir, sizeof(g_run_dir), "%s", s.run_dir ? s.run_dir : "");
    g_info.run_dir = nullptr;
    g_sd = s.sd;   // string literal
    g_utc = utc;
    g_utc_ms = utc_ms;
    portEXIT_CRITICAL(&g_mux);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void BleSync::notifyLive(const Sample& s)
{
    if (state_ == State::Off || !g_live || !g_live_sub || g_conn == BLE_HS_CONN_HANDLE_NONE) { return; }
    uint8_t p[kLiveSize];
    encodeLive(s, p);
    g_live->notify(p, sizeof(p));
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
int BleSync::windowLeftS(uint32_t now) const
{
    if (state_ != State::Advertising) { return 0; }
    if (window_ms_ == 0) { return 0; }
    int32_t left = (int32_t)window_ms_ - diff(now, start_ms_);
    return left > 0 ? (left + 999) / 1000 : 0;
}

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
int BleSync::filePercent() const
{
    if (state_ != State::Sending || win_.size() == 0) { return 0; }
    return (int)((uint64_t)win_.next() * 100 / win_.size());
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void BleSync::finish(End e)
{
    if (end_ == End::None) { end_ = e; }
    changed_ = true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void BleSync::tick(uint32_t now, SdStore& sd)
{
    if (!on() || end_ != End::None) { return; }
    Event e;
    while (xQueueReceive(g_queue, &e, 0) == pdTRUE) {
        switch (e.type) {
        case Ev::Connect:
            state_ = State::Connected;
            had_connection_ = true;
            activity_ms_ = now;
            changed_ = true;
            KF_TRACE("ble: connected");
            break;
        case Ev::Disconnect:
            KF_TRACE("ble: disconnected, reason 0x%02x", e.len ? e.data[0] : 0);
            finish(End::Disconnected);
            return;
        case Ev::Time: {
            activity_ms_ = now;
            int64_t ms;
            if (decodeTimeMs(e.data, e.len, ms)) {
                time_ms_ = ms;
                time_rx_ms_ = e.ms;
                time_pending_ = true;
            }
            break;
        }
        case Ev::Control:
            activity_ms_ = now;
            if (e.len >= 1 && e.data[0] == kCtlFast && e.len == 2) { fast_ = e.data[1] != 0; changed_ = true; }
            else if (e.len >= 1 && e.data[0] == kCtlEnd) { finish(End::ByPage); return; }
            break;
        case Ev::Files:
            activity_ms_ = now;
            KF_TRACE("ble: files request %u B, op 0x%02x, mtu %u", (unsigned)e.len, e.len ? e.data[0] : 0,
                     (unsigned)g_mtu);
            handleRequest(e.data, e.len, now, sd);
            break;
        }
    }

    if (state_ == State::Sending) { pump(now, sd); }

    if (state_ == State::Advertising && window_ms_ && diff(now, start_ms_) >= (int32_t)window_ms_) {
        finish(End::Timeout);
    } else if (state_ != State::Advertising && diff(now, activity_ms_) >= (int32_t)kBleIdleMs) {
        finish(End::Idle);
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void BleSync::handleRequest(const uint8_t* p, size_t n, uint32_t now, SdStore& sd)
{
    Request r;
    Status st;
    uint8_t out[kReadEndSize];
    if (!parseRequest(p, n, r, st)) {
        KF_TRACE("ble: request %u B, op 0x%02x refused, status %d", (unsigned)n, n ? p[0] : 0, (int)st);
        notifyFiles(out, encodeError(st, out), 500);
        return;
    }
    switch (r.op) {
    case kOpList:
        if (state_ == State::Sending) { finishRead(kAborted); }
        sendList(sd);
        break;
    case kOpRead: {
        if (state_ == State::Sending) { finishRead(kAborted); }
        uint32_t size = 0;
        int rc = sd.readChunk(r.path, 0, nullptr, 0, size);
        if (rc < 0) {
            KF_TRACE("ble: read %s refused, sd rc %d", r.path, rc);
            notifyFiles(out, encodeReadEnd(rc == -2 ? kNotFound : kSdError, 0, 0, out), 500);
            return;
        }
        snprintf(path_, sizeof(path_), "%s", r.path);
        win_.begin(r.offset, size);
        read_start_ = win_.next();
        crc_ = 0;
        buf_len_ = 0;
        buf_off_ = 0;
        read_t0_ = now;
        ack_ms_ = now;
        state_ = State::Sending;
        changed_ = true;
        KF_TRACE("ble: read %s from %lu of %lu", path_, (unsigned long)r.offset, (unsigned long)size);
        if (win_.done()) { finishRead(kOk); }
        break;
    }
    case kOpAck:
        if (state_ == State::Sending) {
            win_.ack(r.offset);
            ack_ms_ = now;
        }
        break;
    case kOpAbort:
        if (state_ == State::Sending) { finishRead(kAborted); }
        break;
    default:
        break;
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void BleSync::sendList(SdStore& sd)
{
    static ListPacker lp;   // 0.5 KB, not on the loop task's stack
    lp.begin(maxPacket(g_mtu));
    bool ok = true;
    bool listed = sd.listFiles([&](const char* rel, uint32_t size) {
        if (!ok) { return; }
        if (!lp.add(rel, size)) {
            ok = notifyFiles(lp.packet(), lp.size(), 2000);
            lp.next();
            lp.add(rel, size);
        }
    });
    uint8_t out[kListEndSize];
    if (!listed) {
        notifyFiles(out, encodeError(kSdError, out), 500);
        return;
    }
    if (ok && lp.hasLines()) { ok = notifyFiles(lp.packet(), lp.size(), 2000); }
    if (ok) { notifyFiles(out, lp.encodeEnd(out), 2000); }
    KF_TRACE("ble: list %u files, ok %d", (unsigned)lp.count(), (int)ok);
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Sends for at most 40 ms per loop pass, within the credit window. SD reads are 4 KB
// blocks, each an open/seek/read/close.
void BleSync::pump(uint32_t now, SdStore& sd)
{
    if (diff(now, ack_ms_) >= (int32_t)kBleStallMs) {
        KF_TRACE("ble: no ack for %lu ms, read aborted", (unsigned long)kBleStallMs);
        finishRead(kAborted);
        return;
    }
    static uint8_t pkt[kMaxPacket];
    const size_t payload = dataPayload(g_mtu);
    uint32_t t0 = millis();
    while (!win_.done() && diff(millis(), t0) < 40) {
        uint32_t allowed = win_.allowed();
        if (allowed == 0) { return; }   // wait for an ACK
        uint32_t at = win_.next();
        if (at < buf_off_ || at >= buf_off_ + buf_len_) {
            uint32_t size = 0;
            int n = sd.readChunk(path_, at, buf_, kBufSize, size);
            if (n <= 0) {
                finishRead(n == -2 ? kNotFound : kSdError);
                return;
            }
            buf_off_ = at;
            buf_len_ = (size_t)n;
        }
        size_t chunk = buf_off_ + buf_len_ - at;
        if (chunk > payload) { chunk = payload; }
        if (chunk > allowed) { chunk = allowed; }
        const uint8_t* src = buf_ + (at - buf_off_);
        size_t len = encodeData(at, src, chunk, pkt);
        if (!notifyFiles(pkt, len, 0)) {
            delay(2);   // NimBLE out of buffers: let the host task send
            continue;
        }
        crc_ = crc32Update(crc_, src, chunk);
        win_.sent((uint32_t)chunk);
    }
    if (win_.done()) { finishRead(kOk); }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void BleSync::finishRead(Status st)
{
    uint8_t out[kReadEndSize];
    notifyFiles(out, encodeReadEnd(st, win_.size(), crc_, out), 2000);
    uint32_t ms = millis() - read_t0_;
    uint32_t bytes = win_.next() - read_start_;
    if (st == kOk && ms > 0 && bytes > 0) { last_rate_ = (uint32_t)((uint64_t)bytes * 1000 / ms); }
    KF_TRACE("ble: read %s end %d, %lu B in %lu ms, mtu %u", path_, (int)st, (unsigned long)bytes,
             (unsigned long)ms, (unsigned)g_mtu);
    state_ = (g_conn != BLE_HS_CONN_HANDLE_NONE) ? State::Connected : State::Advertising;
    changed_ = true;
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Retries while NimBLE has no buffer, up to wait_ms.
bool BleSync::notifyFiles(const uint8_t* p, size_t n, uint32_t wait_ms)
{
    if (!g_files || g_conn == BLE_HS_CONN_HANDLE_NONE) { return false; }
    uint32_t t0 = millis();
    while (!g_files->notify(p, n)) {
        if (g_conn == BLE_HS_CONN_HANDLE_NONE || diff(millis(), t0) >= (int32_t)wait_ms) { return false; }
        delay(2);
    }
    return true;
}

} // namespace kf
