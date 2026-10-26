/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// USB-detection bench sketch for the M5Paper.
//
// Every 2 s it reads the USB-detection candidates and appends them to
// /bench_usb.csv on the SD card; the e-ink shows the latest values. Serial is not
// used: on battery nobody listens, and UART0 RX is GPIO3, which is read here as GPIO.
//
// Candidates:
//  - battery: M5Unified voltage (calibrated analogReadMilliVolts, divider 2.0) and the
//    uncalibrated analogRead counts on GPIO35 as an independent check
//  - GPIO3 (U0RXD, driven by the CP2104 TXD): with pull-down, with pull-up, and
//    floating twice (after a pull-down and after a pull-up, 10 ms settle, 16 reads).
//    Driven line: both float counts agree. Floating line: they follow the last pull.
//
// GPIO1 (U0TXD) is set to input at boot. Idling high, it could feed an unpowered
// CP2104 through its protection diodes and make GPIO3 read high on battery.
//
// Two builds:
//  - bench_usb:       stays awake, display every 2 s (simple, high load ~50 mA)
//  - bench_usb_sleep: light sleep between readings, display every 30 s in standby
//                     between refreshes, so the voltage trace is close to the real
//                     firmware's load. Touch wakes the chip and counts as a marker.
//
// Tap the screen to bump the "marker" column right after plugging or unplugging.
// Procedure: per build, about 10 min on USB, 10 min on battery, then a few
// plug/unplug cycles (tap after each). Send me /bench_usb.csv (each boot is marked).

#include <M5Unified.h>
#include <SD.h>
#include <SPI.h>
#include <esp_sleep.h>

namespace {

constexpr int kPinUsbTx = 1;        // U0TXD, CP2104 RXD
constexpr int kPinUsbRx = 3;        // U0RXD, CP2104 TXD
constexpr int kPinBatAdc = 35;
constexpr int kSdCs = 4;
constexpr int kSdSck = 14;
constexpr int kSdMiso = 13;
constexpr int kSdMosi = 12;
constexpr uint32_t kPeriodMs = 2000;
constexpr int kFullRefreshEvery = 30; // GC16 every 30th display update against ghosting
constexpr int kHistoryLines = 14;
#ifdef BENCH_LIGHT_SLEEP
constexpr int kDisplayEvery = 15;    // readings per display update (30 s)
const char* kVariant = "sleep";
#else
constexpr int kDisplayEvery = 1;
const char* kVariant = "awake";
#endif
const char* kLogPath = "/bench_usb.csv";

struct Reading {
    uint32_t uptime_s;
    char rtc[24];
    int bat_mv_m5;
    int bat_adc_counts;
    int gpio3_pd;
    int gpio3_pu;
    int float_after_pd;
    int float_after_pu;
    int marker;
};

bool g_sd_ok = false;
int g_marker = 0;
int g_readings = 0;
int g_updates = 0;
String g_history[kHistoryLines];
int g_history_count = 0;

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Reads GPIO3 with the given pull mode. The settle time lets the internal pull
// (about 45 kOhm) win against pin capacitance when nothing drives the line.
int readGpio3(uint8_t mode)
{
    pinMode(kPinUsbRx, mode);
    delayMicroseconds(200);
    return digitalRead(kPinUsbRx);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Pre-biases GPIO3 with the given pull, releases it, and counts highs over 16 reads.
int readGpio3Floating(uint8_t prebias)
{
    readGpio3(prebias);
    pinMode(kPinUsbRx, INPUT);
    delay(10);
    int highs = 0;
    for (int i = 0; i < 16; ++i) {
        delay(1);
        highs += digitalRead(kPinUsbRx);
    }
    return highs;
}

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
Reading takeReading()
{
    Reading r{};
    r.uptime_s = millis() / 1000;

    auto dt = M5.Rtc.getDateTime();
    snprintf(r.rtc, sizeof(r.rtc), "%04d-%02d-%02dT%02d:%02d:%02d",
             dt.date.year, dt.date.month, dt.date.date,
             dt.time.hours, dt.time.minutes, dt.time.seconds);

    r.bat_mv_m5 = M5.Power.getBatteryVoltage();
    uint32_t sum = 0;
    for (int i = 0; i < 16; ++i) { sum += analogRead(kPinBatAdc); }
    r.bat_adc_counts = (int)(sum / 16);

    r.gpio3_pd = readGpio3(INPUT_PULLDOWN);
    r.gpio3_pu = readGpio3(INPUT_PULLUP);
    r.float_after_pd = readGpio3Floating(INPUT_PULLDOWN);
    r.float_after_pu = readGpio3Floating(INPUT_PULLUP);
    // Leave the pin pulled down so it does not feed an unpowered CP2104.
    pinMode(kPinUsbRx, INPUT_PULLDOWN);

    r.marker = g_marker;
    return r;
}

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Appends one CSV row. Call only after the display has finished (waitDisplay),
// because SD and IT8951 share the SPI bus.
void logReading(const Reading& r)
{
    if (!g_sd_ok) { return; }
    File f = SD.open(kLogPath, FILE_APPEND);
    if (!f) { g_sd_ok = false; return; }
    f.printf("%lu,%s,%s,%d,%d,%d,%d,%d,%d,%d\n",
             (unsigned long)r.uptime_s, r.rtc, kVariant, r.bat_mv_m5, r.bat_adc_counts,
             r.gpio3_pd, r.gpio3_pu, r.float_after_pd, r.float_after_pu, r.marker);
    f.close();
}

/* Edited by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Draws in the refresh mode it will be shown with: M5GFX dithers to 1 bit when the
// mode is epd_fast at draw time, so the mode is set before drawing.
void drawScreen(const Reading& r)
{
    auto& d = M5.Display;
    bool full = (g_updates % kFullRefreshEvery) == 0;
    d.setEpdMode(full ? epd_mode_t::epd_quality : epd_mode_t::epd_fast);
#ifdef BENCH_LIGHT_SLEEP
    d.powerSaveOff();
#endif
    d.startWrite();
    d.fillScreen(TFT_WHITE);
    d.setTextColor(TFT_BLACK, TFT_WHITE);

    d.setTextSize(3);
    d.setCursor(20, 20);
    d.printf("USB bench (%s)  marker %d  SD %s", kVariant, r.marker, g_sd_ok ? "ok" : "MISSING");
    d.setCursor(20, 70);
    d.printf("bat %4d mV  adc %4d counts", r.bat_mv_m5, r.bat_adc_counts);
    d.setCursor(20, 120);
    d.printf("GPIO3 pd %d pu %d  float %2d/%2d", r.gpio3_pd, r.gpio3_pu, r.float_after_pd, r.float_after_pu);

    d.setTextSize(2);
    int y = 180;
    for (int i = 0; i < g_history_count; ++i) {
        d.setCursor(20, y);
        d.print(g_history[i]);
        y += 24;
    }
    d.setCursor(20, 510);
    d.print("Tap screen = marker+1 (tap right after plug/unplug)");
    d.endWrite();

    d.display();
    d.waitDisplay();
#ifdef BENCH_LIGHT_SLEEP
    d.powerSaveOn(); // IT8951 standby, no reset (setSleep would re-init on wake)
#endif
    ++g_updates;
}

/* Edited by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
void pushHistory(const Reading& r)
{
    char line[96];
    snprintf(line, sizeof(line), "%6lus %s bat %4d adc %4d pd%d pu%d fl%2d/%2d m%d",
             (unsigned long)r.uptime_s, r.rtc + 11, r.bat_mv_m5, r.bat_adc_counts,
             r.gpio3_pd, r.gpio3_pu, r.float_after_pd, r.float_after_pu, r.marker);
    if (g_history_count < kHistoryLines) {
        g_history[g_history_count++] = line;
    } else {
        for (int i = 1; i < kHistoryLines; ++i) { g_history[i - 1] = g_history[i]; }
        g_history[kHistoryLines - 1] = line;
    }
}

/* Created by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
// Waits until the next reading. Awake build: polls touch every 20 ms. Sleep build:
// light sleep with GT911 INT (GPIO36) as wake source; a touch wake is a marker.
// M5Unified's lightSleep waits for the finger to lift before sleeping.
void waitForNextReading(uint32_t start_ms)
{
#ifdef BENCH_LIGHT_SLEEP
    while (true) {
        uint32_t elapsed = millis() - start_ms;
        if (elapsed >= kPeriodMs) { return; }
        M5.Power.lightSleep((uint64_t)(kPeriodMs - elapsed) * 1000ULL, true);
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
            ++g_marker;
        }
    }
#else
    while (millis() - start_ms < kPeriodMs) {
        M5.update();
        if (M5.Touch.getCount() && M5.Touch.getDetail().wasPressed()) {
            ++g_marker;
        }
        M5.delay(20);
    }
#endif
}

} // namespace

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void setup()
{
    pinMode(kPinUsbTx, INPUT); // tri-state U0TXD, see header
    auto cfg = M5.config();
    cfg.serial_baudrate = 0;   // no Serial: GPIO3 is read as a GPIO
    M5.begin(cfg);
    pinMode(kPinUsbTx, INPUT); // again, in case M5.begin touched UART0
    M5.Display.setRotation(1);        // landscape 960x540; rotation 0 is portrait on the M5Paper
    M5.Display.setAutoDisplay(false);
    analogReadResolution(12);

    SPI.begin(kSdSck, kSdMiso, kSdMosi, kSdCs);
    g_sd_ok = SD.begin(kSdCs, SPI, 20000000);
    if (g_sd_ok && !SD.exists(kLogPath)) {
        File f = SD.open(kLogPath, FILE_WRITE);
        if (f) {
            f.println("uptime_s,rtc_utc,variant,bat_mv_m5,bat_adc_counts,gpio3_pulldown,gpio3_pullup,float_after_pd_highs16,float_after_pu_highs16,marker");
            f.close();
        } else {
            g_sd_ok = false;
        }
    }
    if (g_sd_ok) {
        File f = SD.open(kLogPath, FILE_APPEND);
        if (f) { f.printf("# boot %s\n", kVariant); f.close(); }
    }
}

/* Edited by Claude Opus 5.5 | Review priority: High | Reviewed by: None */
void loop()
{
    uint32_t start = millis();
    Reading r = takeReading();
    pushHistory(r);
    if (g_readings % kDisplayEvery == 0) {
        drawScreen(r);
    }
    logReading(r);
    ++g_readings;
    waitForNextReading(start);
}
