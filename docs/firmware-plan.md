<!-- Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None -->

# KalliFunghi firmware: architecture plan

## Context
New PlatformIO project in `C:\dev\personal\KalliFunghi\firmware` (folder is empty apart from `docs/firmware-prompt.md`, the spec). Battery logger on an M5Paper with SCD41 + VEML7700 on Grove port A. Goal: lowest battery drain, a 2x2 plot UI on e-ink, SD logging, NTP sync over a phone hotspot.

## Library check (done, sources read 2026-10-23)
| Need | Choice | Why |
|---|---|---|
| Board, touch, RTC, power, SD pins | **M5Unified 0.2.27** | M5Paper fully supported: GT911 on GPIO36 (INT), BM8563, battery ADC GPIO35 ratio 2.0, `Power.setExtOutput()` = EXT5V GPIO5, `Power.lightSleep(us, touch_wakeup)` arms ext0 on GPIO36. |
| E-ink | **M5GFX 0.2.34** (Panel_IT8951) | `setEpdMode`: `epd_quality`=GC16, `epd_text`=GL16, `epd_fast`=DU, `epd_fastest`=DU4; area updates via `display(x,y,w,h)`; `setSleep()` (IT8951 TCON_SLEEP, wake = re-init) and `setPowerSave()` (standby). |
| ~~M5EPD~~ | not used | Archived 2025-07-29, README says use M5Unified/M5GFX. |
| SCD41 | **Sensirion I2C SCD4x 1.1.0** | Has `startLowPowerPeriodicMeasurement`, `setAutomaticSelfCalibrationEnabled`, `performForcedRecalibration`, `setTemperatureOffset`, `getDataReadyStatus`, `powerDown/wakeUp`. |
| VEML7700 | **Adafruit VEML7700 2.1.6** (+BusIO) | Most used, still works; last release 2024 but the part is simple. Shut down between reads (about 0.5 uA). |
| config.json | ArduinoJson 7 | |
| Platform | `platformio/espressif32` (v8.0.0, board `m5stack-paper`, PSRAM flags built in) | Pin the exact version that builds; fall back to pioarduino if M5Unified needs a newer core. |

Pin facts used: internal I2C SDA21/SCL22 (GT911, BM8563, onboard SHT30), port A SDA25/SCL32 (separate bus, `Wire`), SD on the EPD SPI bus (SCK14 MOSI12 MISO13, SD CS4, EPD CS15, busy 27, EPD reset/power 23), main power hold GPIO2.

## Key design decisions
1. **Light sleep, not deep sleep.** RAM (and PSRAM ring buffer) survives; GT911 INT wakes via ext0. Deep sleep would cost a reboot plus IT8951 re-init (~800 ms) per tick and lose the touch path. Revisit only if bench numbers say so.
2. **EXT5V stays on** (SCD41 must run its own periodic cycle). Biggest unknown in the floor current: the GT911 must stay in normal/green mode to wake us on touch. The debug build measures it.
3. **E-ink power** (review H2, M4). GPIO23 is M5GFX's "reset" pin but physically `EPD_PWR_EN`, and `setSleep(false)` calls `init(true)`: every wake from `setSleep` is a power cycle plus ~800 ms boot. So:
   - Interactive / USB: `powerSaveOn()` (TCON_STANDBY) after each refresh, `powerSaveOff()` (SYS_RUN) before the next. No reset, frame buffer kept.
   - Idle (5 min): cut GPIO23 after the refresh; re-init (~800 ms) before the next one; push the full canvas after re-init.
   - Set the EPD mode **before** drawing: in `epd_fast` M5GFX writes 1-bit dithered pixels, so GC16 refreshes draw with `epd_quality`. Partial updates push only the changed regions (value text, plot strips, sidebar), not the full canvas.
   - Refresh only when the shown content changed (USB mode would otherwise refresh every 5 s around the clock and wear the panel).
4. **SD** (review H3, Low). Mount once at boot, remount on error (no power switch, so `SD.end` saves nothing but costs init time). Batched writes every 5 min (or buffer full, mode change, before sync, before power-off). SD I/O only while the EPD is powered; while GPIO23 is low, park CS15 and BUSY27 as inputs so the unpowered IT8951 does not load MISO or get back-fed. A batch that spans UTC midnight splits across two files. If the buffer fills while the card is missing, drop the oldest RAM rows and count them (shown in the sidebar mark).
5. **USB detection behind one interface** (`UsbDetect`), chosen after the bench run (review H4).
   - GPIO1 (U0TXD) is tri-stated whenever on battery, so it cannot feed the unpowered CP2104.
   - If GPIO3 is reliable: use it, read on every tick.
   - Voltage heuristic fallback, measured at a fixed load point (right after wake, before radio/EPD): enter USB on +30 mV over 10 min or >= 4.15 V; leave on any sustained drop over 3 readings, and after at most 2 h in USB_PERF re-confirm (one Eco-load measurement window, 10 min). Unplug detection delay with this rule: typically 10 to 30 min, worst case 2 h 10 min; documented in the README. Per-unit ADC error (±40 to 80 mV) makes the 4.15 V threshold soft, hence the drop rule.
   - **Hardware option for you to decide**: 100k/100k divider from VBUS at the USB-C connector to a free ADC1 pin (e.g. GPIO34 or GPIO26 if free on the M5Paper header), 25 uA only while USB is present. Unambiguous, no heuristics.
6. **Unsynced rows**: add a 7th column `clock` = `ntp` or `unsynced` (header `utc_iso,co2_ppm,temp_c,rh_pct,lux,mode,clock`). Keeps `utc_iso` parseable; fix-up script just filters on the column. "Unsynced" = no sync stored in NVS or BM8563 VL (low-voltage) flag set.
7. **Tap vs double tap** (window 400 ms): a tap on a target with no single-tap action (plot area in full-screen view, empty space) is not deferred; any double tap goes home immediately on the second tap. A tap on an actionable target (plot on home, Sync, Settings, settings controls) is deferred 400 ms; during the window I already render the next view into the PSRAM canvas, so only the panel push waits. Since GC16 takes ~450 ms anyway, the delay is barely visible. **The chip stays awake during the 400 ms window** (review M1: `M5.Power.lightSleep` waits for INT release and discards GT911 data, so short sleeps would eat the second tap). Before any light sleep: `M5.update()`, sleep only when no finger is down. Touches during a panel refresh are queued and handled after `waitDisplay()`. Bench check: GT911 INT pulses vs ext0 level wake (green mode scans every 40 ms).
8. **SCD41 command handling** (review M2).
   - Boot: `stopPeriodicMeasurement` + 500 ms, then ASC off and temperature offset (both only accepted in idle), then start the mode. ASC=0 is additionally persisted once (`persistSettings`, only if the sensor reports ASC on; EEPROM is rated 2000 writes) so a brown-out cannot silently re-enable ASC.
   - Offset change at runtime: stop, 500 ms, set, start, drop first reading.
   - Mode change (incl. USB plug/unplug): stop, 500 ms, start the other mode, drop first reading.
   - Reads follow the sensor's own cycle: wake, `getDataReadyStatus`; if not ready, light sleep 1 s and retry (max 5 s), so the tick never reads stale data.
   - Health: no data for 3 intervals or I2C error -> `SENSOR_REINIT` (stop, re-apply settings, restart); the plot shows "no sensor" until data returns.
   - FRC: allowed only after >= 3 min of continuous operation in the current mode (datasheet precondition); the Calibrated button shows a countdown until then. Sequence: stop, 500 ms, `performForcedRecalibration(420)` (0xFFFF = failed), restart, drop first reading.
9. **Plots**: 6 h ring in PSRAM (Fast worst case 4320 samples x 16 B ~ 70 kB). 24 h / 7 d stream the daily CSVs once into per-column min/max bins (no full load). Downsampling is a pure function, unit tested.
10. **Weekly calibration reminder** (non-blocking). NVS stores `last_cal_utc` and `cal_snooze_until_utc`. When `now - last_cal_utc >= 7 d` (constant in `config.h`) and `now >= cal_snooze_until_utc`, an info box (about 80 px strip at the bottom of the plot area on Home and full-screen plot views; plots shrink, everything keeps running) reads: "CO2 calibration due. Place the device outdoors for 30 min, then press Calibrated." Buttons:
    - **Calibrated**: runs the forced recalibration (stop, 500 ms, `performForcedRecalibration(420)`, restart in current mode, drop first reading), shows the correction in ppm, sets `last_cal_utc = now`. With ASC off, the outdoor exposure alone changes nothing, so this button must do the FRC. If the FRC fails, the box stays with the error.
    - **Postpone**: `cal_snooze_until_utc = now + 24 h`.
    **Calibrate now (new batch)**: big button on the Settings page (replaces the plain FRC entry), usable any time, typically right before a new batch. It opens the same info box ("Place the device outdoors for 30 min, then press Calibrated", plus Cancel); Calibrated runs the FRC and restarts the 7-day timer exactly as above. On first boot `last_cal_utc` = now (no reminder on day 1). A small mark in the sidebar while the box is shown. The box only appears with the next normal refresh (no extra wake).
11. **Low battery** (review H6). M5Unified's percent is `(mV - 3300) / 8`, so "5 %" is 3.34 V, which is already empty. Use a voltage threshold instead: 3.55 V at the fixed load point, 3 consecutive readings, then final GC16 screen "Battery low, last update <time>", flush SD, `M5.Power.powerOff()`. After boot, start only if >= 3.65 V (hysteresis, no boot/off loop from cell recovery). Battery percent is shown as "USB" while USB is present (meaningless while charging). Wi-Fi sync is blocked below 3.7 V (250 mA peaks).
12. **Power hold** (review M5). Drive GPIO2 high and `gpio_hold_en(GPIO2)` at boot, so a panic, watchdog or brown-out reset on battery does not cut power. Test `esp_restart()` on battery.
13. **Monotonic scheduling** (review M3). All timers (tick, refresh, interactive timeout, tap window, auto-sync) run on `esp_timer` (survives light sleep). The BM8563 (1 s resolution, UTC) is used only for timestamps, file names and day boundaries. Times stored in NVS while unsynced (`last_cal_utc`, snooze) are rebased by the correction at the first sync. Until the first sync, rows go to `/log/unsynced.csv` instead of a `2000-01-01.csv` file. Ring-buffer samples store a monotonic time plus UTC; plots use the monotonic axis until synced.
14. **Missing sensor in CSV**: empty field (e.g. `2026-10-24T10:00:00Z,,21.3,88.0,140,eco,ntp`).
15. **Sync failure reasons**: map Wi-Fi disconnect reasons: NO_AP_FOUND -> "hotspot not found"; AUTH_FAIL, 4WAY_HANDSHAKE_TIMEOUT, HANDSHAKE_TIMEOUT -> "wrong password"; no NTP reply from both servers -> "NTP timeout".
16. **VEML7700** (review Low): enable, sleep one integration time plus margin (fixed gain/IT chosen for grow-box light, one step of auto-range if saturated or below range), read, shut down. No Adafruit `VEML_LUX_AUTO` (can take seconds).
17. **Ultra-Eco mode (decided 2026-10-23: build it, Eco stays default).** Third sampling mode in Settings (Ultra-Eco / Eco / Fast), CSV `mode` value `ultra`.
    | | Ultra-Eco | Eco (default) | Fast |
    |---|---|---|---|
    | SCD41 | single shot every 5 min | low-power periodic, 30 s | periodic, 5 s |
    | VEML7700 | every 5 min (same tick) | every 30 s | every 5 s |
    | Display, idle | every 5 min | every 5 min | every 5 min |
    | Display, interactive | every 30 s, only if content changed (clock, sidebar) | every 30 s | every 5 s |
    - Non-blocking single shot: issue `measure_single_shot` (0x219D), light sleep 5 s, `readMeasurement`. The library's `measureSingleShot()` blocks 5 s awake, so it is not used.
    - Between shots the SCD41 stays in idle (no `powerDown`: after `wakeUp` the first reading must be discarded, which would double the shots). Datasheet average for this pattern: 0.36 mA at 5 V.
    - Mode change into or out of Ultra-Eco: `stopPeriodicMeasurement` + 500 ms when leaving a periodic mode; the first single shot after the change is discarded.
    - FRC in Ultra-Eco: the datasheet wants >= 3 min in the operating mode first, so Calibrate runs back-to-back single shots for 3 min (36 shots), then the FRC, then back to the 5 min cycle.
    - USB still forces Fast; on unplug it returns to Ultra-Eco if that is the user's choice.
    - The 6 h plot then has 72 points; plots draw them as connected points (no min/max bins needed).

## Power state machine (`src/power/PowerManager.*`, priority High)
Display/power states: `BOOT -> IDLE <-> INTERACTIVE`, `USB_PERF` (from any when USB present, back to IDLE on unplug or failed re-confirm), `SYNCING` (temporary, returns to the state it came from), `LOW_BATT_OFF` (terminal). Sensor states, orthogonal: `RUNNING`, `MODE_CHANGE` (stop/500 ms/start), `CALIBRATING` (FRC), `SENSOR_REINIT`. Sampling mode is derived: `USB ? Fast : user setting`.

Rules for overlaps (review M6):
- SYNCING (max 20 s / 10 s) runs as a sub-loop that still takes due samples; touches are ignored except a double tap (cancel). A USB change during sync is applied when sync ends. Sync is refused below 3.7 V and during CALIBRATING.
- A mode change requested during CALIBRATING waits until the FRC is done.
- Low battery wins over everything: flush, final screen, power off.

Each wake: check USB (cheap read), read BM8563 for the timestamp, handle due events, then compute the next wake on `esp_timer` = min(next sample, next refresh, interactive timeout, daily auto-sync check) and light sleep (no sleep while a tap window is open or a finger is down). ASCII diagram goes in the header comment.

## File layout
```
firmware/
  platformio.ini        envs: m5paper, m5paper_debug (-DKF_DEBUG_POWER), bench_usb, bench_usb_sleep, native (tests; overrides the inherited board and lib_deps)
  README.md             build, flash, config.json, expected battery life, warm-back note, unverified list
  src/main.cpp          setup/loop -> App
  src/App.{h,cpp}       wires modules, owns the tick loop
  src/config.h          pins, timings, defaults (SSID, NTP servers, TZ)
  src/power/            PowerManager, UsbDetect, Battery, DebugTrace (wake time, awake ms, tick counts)
  src/sensors/          Scd41Sensor, LightSensor (VEML7700), SensorHub (per-sensor ok/missing)
  src/data/             SampleRing (PSRAM), Downsample (pure), Sample.h
  src/storage/          SdLogger (batched CSV, sd-missing/full flag), SdHistory (range rebuild), Settings (NVS), ConfigFile (config.json)
  src/time/             Clock (BM8563 UTC, TZ CET/CEST), TimeSync (WiFi + NTP, 20 s / 10 s, result codes)
  src/sensors/CalReminder.{h,cpp}   due/snooze logic (pure, unit tested)
  src/ui/               Display (EPD power, DU/GC16, every 10th partial -> GC16), TouchInput (tap/double tap), Sidebar, HomeView, PlotView, PlotRenderer, SettingsView, CalInfoBox, UiController
  bench/usb_detect/main.cpp   bench sketch (env bench_usb)
  test/test_native/     Downsample, TouchInput classifier, USB heuristic, PowerManager transitions, CSV row format
```
Logic modules (TouchInput, PowerManager, Downsample, USB heuristic, CSV formatting) stay free of Arduino headers so they run in `pio test -e native`. Every file/function gets the CLAUDE.md tag (owner Julian Martus); sleep, interrupts, timing, SPI sharing, RTC, power = High.

## Order of work
1. Project skeleton + `platformio.ini`, confirm it builds.
2. **Bench sketch** (done, builds), two envs: `bench_usb` (awake) and `bench_usb_sleep` (light sleep, display every 30 s in standby, load close to the firmware). Every 2 s to `/bench_usb.csv`: battery mV (M5Unified) and raw ADC counts, GPIO3 with pull-down, with pull-up, and floating after each pull (driven vs floating line). GPIO1 tri-stated at boot. Tap = marker. You run both with and without USB plus plug/unplug cycles and report. Please also measure, if you can: board current with the bench supply in `bench_usb_sleep`, and whether a short tap reliably wakes it (GT911 INT pulse vs level wake). **I stop here for your result.**
3. Then the full firmware in the order: time/settings/storage, sensors, power state machine, UI, sync, low battery, debug trace, tests, README with current estimates.

## Expected average current (first estimate, to be replaced by bench data)
Corrected after review H1 (SCD41 datasheet Table 4: low-power periodic 2.8 mA, periodic 11 mA, single shot every 5 min 0.36 mA, all at 5 V). Battery side via the 5 V boost (~85 %, 3.8 V cell): SCD41 ~4.3 mA (Eco), ~17 mA (Fast), ~0.6 mA (Ultra-Eco). Board floor (ESP32 light sleep 0.8 mA, GT911 green mode, PSRAM, regulators, boost quiescent, SD standby) assumed 2.5 to 5 mA, the first thing to measure. Wake 60 ms at 40 mA per tick, GC16 with re-init ~150 mAs, DU from standby ~30 mAs.
- Eco, idle: ~7.5 to 10 mA -> about 5 to 6 days on 1150 mAh.
- Fast, idle (and USB): ~21 to 23 mA -> about 2 days.
- Ultra-Eco: ~3.5 to 6 mA -> about 8 to 14 days.
- Interactive adds ~1 mA (Eco, 30 s) or ~6 mA (Fast, 5 s) while active.
The SCD41 dominates in Eco and Fast; the board floor dominates in Ultra-Eco.

## Verification
- `pio run -e m5paper`, `pio run -e bench_usb`, `pio test -e native` on this machine.
- On hardware (you): bench sketch result; debug build serial log + bench supply current per mode; touch tap/double-tap feel; SD pull-out while running; sensor unplug; NTP sync with hotspot on/off/wrong password.

## Cannot verify without hardware
GT911 green-mode current and short-pulse wake reliability, IT8951 standby vs power-off current, back-feed current with GPIO23 low, SD idle current, GPIO3 USB behavior, real battery life, SCD41 self-heating offset, timing of DU vs GC16 on this panel, whether GPIO2 hold keeps the board on through a reset.

## Changes during implementation (2026-10-23)
- **USB heuristic** (tests showed two flaws in the planned rules): "below 4.10 V" is now a downward crossing, not a level (charging from a low cell runs below 4.10 V). The flat-high entry (>= 4.15 V, flat) only applies until the first exit, because a just-unplugged full cell also sits flat above 4.15 V. The 2 h re-confirmation was dropped: with a full cell the charger holds the voltage constant at any load, so a re-check cannot tell USB from battery. Limits are documented in `UsbHeuristic.h` and the README.
- **Rendering** draws straight into the IT8951 instead of a PSRAM canvas, and always redraws the whole view (needed anyway after a panel power-off). The tap window therefore only delays the push; drawing takes tens of ms. Refreshes are skipped when a hash of the visible content is unchanged.
- **Pure logic** lives in `lib/kfcore` (shared by firmware and tests); `config.h` moved there.
- **Debug trace** goes to Serial2 TX on Grove port C (GPIO18), so current can be measured on battery without USB.
- **VEML7700**: fixed gain 1/8, 100 ms (0.46 lux resolution, up to about 30 klux), no auto-range step.
- **Calibration**: Calibrated is always pressable; if the 3 min warm-up is not met yet it starts a countdown and runs the FRC when it ends (in Ultra-Eco: back-to-back single shots during the countdown).
- **Unit tests** compile and link for the ESP32 (`pio test -e m5paper` runs them on the device); the `native` env needs a host gcc, which this machine does not have.
