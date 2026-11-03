/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Bluetooth sync session: NimBLE GATT server for the Web Bluetooth page (protocol in
// kfcore/BleProto.h, UUIDs in config.h, README "Bluetooth sync").
//
// Lifetime: start() brings the controller up and advertises; the session ends when the
// advertising window passes without a connection (window_ms, 0 = no limit), at the
// disconnect, after kBleIdleMs without page activity, on the page's "end" or on stop().
// stop() deinitialises NimBLE completely, so the radio is off and its RAM is free again.
// One connection at a time. The App starts the windows (App::bleTick).
//
// Threads: NimBLE callbacks run in the NimBLE host task. They only push events into a
// FreeRTOS queue and update atomics; everything that touches the SD card (shared SPI
// with the e-ink), the RTC (I2C) or App state runs in tick(), in the App loop.
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "BleProto.h"
#include "Types.h"

namespace kf {

class SdStore;

class BleSync {
public:
    enum class State : uint8_t { Off, Advertising, Connected, Sending };
    enum class End : uint8_t { None, Timeout, Disconnected, Idle, ByPage, Failed };

    // false if the stack could not start (radio stays off).
    bool start(uint32_t now, uint32_t window_ms);
    void stop();
    bool on() const { return state_ != State::Off; }
    State state() const { return state_; }
    // A READ is running: the App holds back routine SD flushes.
    bool transferring() const { return state_ == State::Sending; }

    // Loop step: requests, transfer pump, timeouts.
    void tick(uint32_t now, SdStore& sd);
    // The session ended by itself (timeout, disconnect, page): the App calls stop().
    End ended() const { return end_; }
    // UI-relevant change since the last call (connect, transfer start/end).
    bool takeChanged();

    // Time written by the page: UTC ms plus the millis() when it arrived.
    bool takeTime(int64_t& utc_ms, uint32_t& rx_ms);
    bool fastWanted() const { return fast_; }
    // A page connected during this session (the App only reports such sessions).
    bool hadConnection() const { return had_connection_; }
    bool connected() const { return state_ == State::Connected || state_ == State::Sending; }

    // Snapshot for Status reads. utc/ms is the App's RTC reading and its millis().
    void setStatus(const bleproto::StatusInfo& s, int64_t utc, uint32_t utc_ms);
    void notifyLive(const Sample& s);

    // Seconds until the advertising window ends; 0 while connected or without a limit.
    int windowLeftS(uint32_t now) const;
    const char* fileName() const { return path_; }
    int filePercent() const;
    // Measured rate of the last finished READ (bytes/s), 0 if none.
    uint32_t lastRate() const { return last_rate_; }

private:
    void handleRequest(const uint8_t* p, size_t n, uint32_t now, SdStore& sd);
    void sendList(SdStore& sd);
    void pump(uint32_t now, SdStore& sd);
    void finishRead(bleproto::Status st);
    bool notifyFiles(const uint8_t* p, size_t n, uint32_t wait_ms);
    void finish(End e);

    State state_ = State::Off;
    End end_ = End::None;
    bool changed_ = false;
    uint32_t start_ms_ = 0;
    uint32_t window_ms_ = 0;     // advertising window, 0 = until stop()
    uint32_t activity_ms_ = 0;
    bool fast_ = false;
    bool had_connection_ = false;

    bool time_pending_ = false;
    int64_t time_ms_ = 0;
    uint32_t time_rx_ms_ = 0;

    // Running READ
    char path_[bleproto::kPathMax] = "";
    bleproto::ReadWindow win_;
    uint32_t crc_ = 0;
    uint32_t read_start_ = 0;     // first offset of this transfer
    uint32_t read_t0_ = 0;
    uint32_t ack_ms_ = 0;
    uint32_t last_rate_ = 0;
    static constexpr size_t kBufSize = 4096;
    uint8_t* buf_ = nullptr;      // kBufSize, allocated while the session is on
    uint32_t buf_off_ = 0;        // file offset of buf_[0]
    size_t buf_len_ = 0;
};

} // namespace kf
