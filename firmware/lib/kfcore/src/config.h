/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Pins, timings and defaults. Times are milliseconds unless the name says otherwise.
#pragma once

#include <stdint.h>

#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

namespace kf {

// ---- Pins (M5Paper, checked against M5Unified 0.2.27 / M5GFX 0.2.34) ----
constexpr int kPinMainPowerHold = 2;   // high keeps the board powered on battery
constexpr int kPinExt5vEnable = 5;     // Grove 5 V boost (SCD41, VEML7700)
constexpr int kPinEpdPower = 23;       // M5GFX "reset" pin, physically EPD_PWR_EN
constexpr int kPinEpdCs = 15;
constexpr int kPinEpdBusy = 27;
constexpr int kPinTouchInt = 36;       // GT911 INT, light-sleep wake source
constexpr int kPinBatteryAdc = 35;
constexpr int kPinUart0Tx = 1;         // CP2104 RXD
constexpr int kPinUart0Rx = 3;         // CP2104 TXD, USB-detect candidate
constexpr int kPinPortASda = 25;
constexpr int kPinPortAScl = 32;
constexpr int kPinSdCs = 4;
constexpr int kPinSpiSck = 14;
constexpr int kPinSpiMosi = 12;
constexpr int kPinSpiMiso = 13;
constexpr uint32_t kPortAI2cHz = 100000;
constexpr uint32_t kSdSpiHz = 20000000;

// ---- USB detection ----
// GPIO3 detection is only trustworthy after the bench run (bench/usb_detect).
// Until then the battery-voltage heuristic is used.
enum class UsbMethod : uint8_t { BatteryHeuristic, Gpio3 };
constexpr UsbMethod kUsbMethod = UsbMethod::BatteryHeuristic;
constexpr int kUsbEnterMv = 4150;              // at or above: USB
constexpr int kUsbLeaveMv = 4100;              // below: battery
constexpr int kUsbRiseMv = 30;                 // rise over the window: USB
constexpr int kUsbDropMv = 20;                 // drop below the window max: battery
constexpr uint32_t kUsbWindowMs = 10UL * 60 * 1000;
constexpr uint32_t kUsbHeuristicStepMs = 60UL * 1000;   // one voltage point per minute

// ---- Battery ----
constexpr int kBatteryLowMv = 3550;            // measured at the fixed load point
constexpr int kBatteryLowCount = 3;            // consecutive low readings
constexpr int kBatteryBootMinMv = 3650;        // hysteresis after a low-battery power-off
constexpr int kBatterySyncMinMv = 3700;        // no Wi-Fi below this
constexpr int kBatteryEmptyMv = 3550;          // 0 % on the sidebar
constexpr int kBatteryFullMv = 4150;           // 100 % on the sidebar

// ---- Sampling ----
constexpr uint32_t kSampleFastMs = 5000;
constexpr uint32_t kSampleEcoMs = 30000;
constexpr uint32_t kSampleUltraMs = 5UL * 60 * 1000;
constexpr uint32_t kScdStopWaitMs = 500;       // datasheet stop_periodic_measurement
constexpr uint32_t kScdSingleShotMs = 5000;
constexpr uint32_t kScdDataReadyRetryMs = 1000;
constexpr uint32_t kScdDataReadyMaxWaitMs = 5000;
constexpr int kScdMissedIntervalsReinit = 3;
constexpr uint32_t kScdFrcWarmupMs = 3UL * 60 * 1000;   // FRC precondition
constexpr uint16_t kFrcTargetPpm = 420;
// Pressure compensation for the CO2 reading (sensor default 0 m = sea level).
// Heidelberg old town about 114 m; change if the box stands much higher.
constexpr uint16_t kSensorAltitudeM = 115;
constexpr uint32_t kVemlSettleMs = 130;        // 100 ms integration + wake-up + margin

// ---- Display ----
// M5GFX rotation for 960x540 landscape. Rotation 0 is portrait on the M5Paper.
// 1 or 3: use the other one if the screen is upside down.
constexpr int kDisplayRotation = 1;
constexpr uint32_t kRefreshIdleMs = 5UL * 60 * 1000;
constexpr uint32_t kRefreshInteractiveFastMs = 5000;
constexpr uint32_t kRefreshInteractiveEcoMs = 30000;
constexpr uint32_t kInteractiveTimeoutMs = 5UL * 60 * 1000;
constexpr int kPartialsBeforeFull = 10;
constexpr int kBootCleanCycles = 2;    // black/white GC16 cycles at every start
// IT8951 VCOM in mV (positive number for a negative voltage). M5EPD always set
// -2.30 V; M5GFX sets nothing by default, which left this panel at 0 V and caused
// strong ghosting. Use the value printed on the panel ribbon if it differs.
constexpr uint16_t kEpdVcomMv = 2300;
constexpr uint32_t kDoubleTapWindowMs = 400;
constexpr int kScreenW = 960;
constexpr int kScreenH = 540;
constexpr int kSidebarW = 130;

// ---- Logging and plots ----
constexpr uint32_t kSdFlushMs = 5UL * 60 * 1000;
constexpr int kLogBufferRows = 256;            // rows held in RAM between flushes
constexpr uint32_t kRingSeconds = 6UL * 3600;
constexpr int kRingCapacity = kRingSeconds / (kSampleFastMs / 1000) + 64;
constexpr uint64_t kSdMinFreeBytes = 1024ULL * 1024;

// ---- Time and sync ----
constexpr const char* kTimezone = "CET-1CEST,M3.5.0,M10.5.0/3";
// Hotspot defaults (kDefaultSsid, kDefaultPassword) come from secrets.h, which is
// not in git. Without it the build uses secrets.example.h (placeholders) and the
// hotspot must be set in /config.json on the SD card.
constexpr const char* kNtpServer1 = "pool.ntp.org";
constexpr const char* kNtpServer2 = "ptbtime1.ptb.de";
constexpr uint32_t kSyncManualTimeoutMs = 20000;
constexpr uint32_t kSyncAutoTimeoutMs = 10000;
constexpr uint32_t kAutoSyncCheckMs = 24UL * 3600 * 1000;
constexpr int64_t kAutoSyncAgeS = 7LL * 86400;
constexpr int64_t kSyncWarnAgeS = 30LL * 86400;

// ---- Calibration reminder ----
constexpr int64_t kCalIntervalS = 7LL * 86400;
constexpr int64_t kCalSnoozeS = 24LL * 3600;

// ---- Settings limits ----
constexpr float kTempOffsetMin = 0.0f;
constexpr float kTempOffsetMax = 10.0f;
constexpr float kTempOffsetStep = 0.5f;

} // namespace kf
