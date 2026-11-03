<!-- Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None -->

# KalliFunghi firmware

Battery-powered environment logger for a mushroom grow box: CO2, temperature, humidity and light, plotted on e-ink and logged to SD.

- **Board:** M5Stack M5Paper (ESP32, 8 MB PSRAM, 4.7" e-ink 960x540, GT911 touch, BM8563 RTC, SD, 1150 mAh LiPo).
- **Sensors:** Sensirion SCD41 (CO2, temperature, humidity) and Vishay VEML7700 (light), both on I2C via Grove port A (SDA 25, SCL 32), for example through a Grove hub.
- **Libraries:** M5Unified 0.2.27 and M5GFX 0.2.34 (M5EPD is archived), Sensirion I2C SCD4x 1.1.0, Adafruit VEML7700 2.1.6, ArduinoJson 7. Platform `espressif32 @ 8.0.0` (Arduino core 3.3).

## Build and flash

PlatformIO Core is enough (`~/.platformio/penv/Scripts/pio.exe` on this machine).

The hotspot name and password are not in git. Copy `lib/kfcore/src/secrets.example.h` to `secrets.h` in the same folder and fill them in. Without `secrets.h` the build still works, but uses placeholders; then set the hotspot in `/config.json` on the SD card.

| Env | What |
|---|---|
| `m5paper` | Firmware |
| `m5paper_debug` | Firmware plus power trace on Grove port C (see Debug) |
| `bench_usb`, `bench_usb_sleep` | USB-detection bench sketch (`bench/usb_detect`) |
| `epd_clean` | One-off e-ink cleaning: sets VCOM, runs INIT and black/white cycles (`bench/epd_clean`) |
| `native` | Unit tests of `lib/kfcore` on the PC (needs gcc/g++ on PATH) |

```bash
pio run -e m5paper -t upload
pio test -e m5paper
pio test -e native
```

`pio test -e m5paper` runs the same unit tests on the M5Paper over serial.

## Use

- **Home:** 2x2 plots of CO2 (ppm), temperature (degC), humidity (%RH) and light (lux). Each shows the current value, min/max over the range, axis labels and local time ticks.
- **Tap a plot:** full screen. **Double tap anywhere:** back to home.
- **Sidebar:**
  - local time and date
  - battery percent, or "USB"
  - mode (Ultra / Eco / Fast); `*` = interactive
  - age of the last sync
  - Home view only: age of the last CO2 calibration (`CO2 cal 12d`, `<1d` or `never`) and the run (`Run 4, day 12`; day 1 is the start date, local time)
  - marks: `! Clock` (never synced, or last sync more than 30 days ago), plug icon (USB power), `! SD` (card missing, full or failing), `Cal due`
  - button **Settings** (and **Sync** only with `kWifiSync`)
- **Settings:** plot range (1 h, 6 h, 24 h, 7 d), sampling mode, auto-sync on/off (only with `kWifiSync`), SCD41 temperature offset (0 to 10 degC in 0.5 steps), **Calibrate CO2 now** (with the date of the last calibration), **New measurement** (with the current run and its start date).
- **Interactive mode:** any touch starts it for 5 min; every touch restarts the timer.

### Tap vs double tap

The double-tap window is 400 ms.
- A tap on something that does nothing on a single tap (the full-screen plot body, empty space) is not delayed. It only counts as the first half of a possible double tap.
- A tap on a button or a home plot waits until the 400 ms window has passed. A second tap inside the window means "home", and the single-tap action is never run.

The chip stays awake during the window (no light sleep), because M5Unified's light sleep drops touch data.

### CO2 calibration (SCD41)

Automatic self-calibration (ASC) is off: a grow box rarely sees fresh air, so ASC would drift the readings. Instead there is a forced recalibration to 420 ppm:
- **Weekly reminder:** a box at the bottom of the plots says "CO2 calibration due". Place the device outdoors for 30 min, then press **Calibrated**. **Postpone** hides the box for 24 h. The device keeps working normally while the box is shown.
- **Calibrate CO2 now:** in Settings at any time. Same flow, with **Cancel**. **New measurement** offers it too.

"Last calibrated" only counts a successful forced recalibration. The reminder interval starts at the last calibration, or at the first boot if there was none (NVS `calBase`). Firmware before 2026.11.03 stored the first boot time as the calibration time; at the first boot with this firmware that value becomes `calBase` and the calibration counts as "never" (no earlier firmware logged calibrations, so a real one cannot be told apart). Every calibration is written to `error.log` and as `calibration=` to the run's `run.txt`.

The datasheet wants at least 3 min of continuous measurement in the current mode before the recalibration. In Eco and Fast that is normally already the case, so it runs at once. In Ultra-Eco, pressing Calibrated starts 3 min of back-to-back single shots first: keep the device outdoors during the countdown. The result (correction in ppm) is shown for 2 min.

### Altitude

The SCD41's CO2 reading depends on air pressure. The firmware sets the sensor altitude to 115 m (Heidelberg) on every start. If the box stands much higher, change `kSensorAltitudeM` in `lib/kfcore/src/config.h`.

### Temperature offset

The M5Paper back gets warm (ESP32, charger, e-ink driver), which makes the SCD41 read too high and the humidity too low. Mount the sensor away from the M5Paper, on a cable. Then compare against a reference thermometer and set the offset in Settings. The offset is applied in the sensor and is re-applied on every boot.

### E-ink VCOM and ghosting

M5GFX does not set the IT8951 VCOM (panel drive voltage) unless told to. On this board it read 0 V, which caused strong ghosting and burned-in marks. M5Stack's old M5EPD library always set -2.30 V. The firmware now sets `kEpdVcomMv` (2300 = -2.30 V) at every start, and M5GFX re-applies it after every panel power-up. If your panel's ribbon cable shows a different value, use that one.

Every start also runs 2 black/white cycles (`kBootCleanCycles`). For stubborn marks, flash `epd_clean` once: it runs the IT8951 INIT waveform, which M5GFX keeps private. Then flash the firmware again.

## Sync

The clock is set over Bluetooth from the web page (see "Bluetooth sync"). The Wi-Fi hotspot sync below is compiled out for now: `kWifiSync = false` in `config.h` hides the **Sync** button, the Auto-sync row in Settings and skips auto-sync. The code still builds; `true` brings it back.

### Hotspot (with `kWifiSync`)

- **Sync** connects to the phone hotspot, gets NTP time (`pool.ntp.org`, fallback `ptbtime1.ptb.de`), writes the BM8563 (UTC), then turns Wi-Fi off. The timeout is 20 s.
- The result is shown at the bottom: the correction in seconds, or "Hotspot not found", "Wrong password", "NTP timeout" or "Wi-Fi connect failed".
- **Auto-sync** (default on): 5 min after boot and then once a day, if the last sync is more than 7 days old, it tries silently for 10 s.
- No sync below 3.7 V on battery, and none during a calibration.

### config.json

Optional, in the SD card root. It overrides the compiled-in hotspot:

```json
{
  "wifi": {
    "ssid": "your-hotspot",
    "password": "your-password"
  }
}
```

## Bluetooth sync

Clock sync, status, live samples and SD download for the static Web Bluetooth page in `web/` (repo root). NimBLE-Arduino 2.5.1, peripheral only. Code: `src/ble/BleSync` (radio, transfer pump), `lib/kfcore/src/BleProto` (byte formats, unit tested).

### Session

- **Advertising** as `KalliFunghi` (service UUID in the advertisement, name in the scan response), no tap needed:
  - **USB power:** always (restarted 2 s after a session ends). When USB goes away, the unlimited window ends unless a page is connected.
  - **Battery:** a window of 30 s every 5 min (`kBleWindowMs`, `kBlePeriodMs`), from 3.60 V up (`kBatteryBleMinMv`). The first window opens 10 s after boot, after a brown-out reset only after one full period (no reset loop on a weak cell).
  - A window without a connection is silent: no redraw, no SD or NVS write. Each window is one `NimBLEDevice::init` / `deinit(true)` (controller off and stack RAM freed in between, so the chip can light-sleep).
- **Connected:** the radio stays on until the disconnect, 10 min without page activity (keepalive), the page's "end", **Stop** on the device, or below 3.55 V on battery. A connected page counts as interactive (`psm_.onTouch`); the bottom strip shows "connected", the file being sent and the last result, with **Stop**. Connect, time sync and session end go to `error.log`. One connection at a time.
- **Power:** no light sleep while Bluetooth is on: the prebuilt controller uses the main crystal as its sleep clock (`CONFIG_BTDM_CTRL_LPCLK_SEL_MAIN_XTAL`), so light sleep would stop it (derived from the sdkconfig, not measured). The loop polls with `delay()` (1 ms while sending, 20 ms otherwise). On battery the low-battery count and the USB heuristic pause while the radio is on (the voltage sags under load). Cost estimate: 30 s at ~35 mA every 5 min is about +3 mA average, not measured.
- **Security:** no pairing or bonding (decided): anyone in range can connect during a window, set the clock and download the logs. The data are not secret; one connection at a time.
- **Fast while connected:** off by default; the page's checkbox switches the effective mode to Fast (not saved, not during a calibration) and back at the end.

### GATT

Service `4b460000-6b61-6c6c-6966-756e67686900`. Integers little endian.

| UUID | Name | Properties | Content |
|---|---|---|---|
| `4b460001-6b61-6c6c-6966-756e67686900` | Status | read | JSON: `fw, utc_ms, clock_valid, last_sync, bat_pct, bat_mv, usb, mode, eff_mode, sd (ok/missing/full/error), free_kb, run_no, run_dir, run_start, last_cal, window_s, live_fast`. Booleans are 0/1, times UTC seconds (0 = never), `utc_ms` is computed at the read. |
| `4b460002-...` | Time | write | int64 UTC ms. The device waits for the next full second, writes the BM8563 and records the sync like the Wi-Fi sync (`last_sync_utc`, `sync.csv`, `run.txt clock_synced=` on the first sync, rebase of unsynced data, message "Sync ok, clock corrected by N s"). Resolution: the RTC counts whole seconds, so expect up to about 1 s. |
| `4b460003-...` | Live | notify | 16 B per new sample: `utc u32, co2 u16, temp_c100 i16, rh_c100 u16, lux f32, mode u8 (0 ultra, 1 eco, 2 fast), flags u8 (1 unsynced, 2 SCD41 valid, 4 lux valid)` |
| `4b460004-...` | Files | write, notify | Request / answer protocol below |
| `4b460005-...` | Control | write without response | `01` keepalive, `02 on` Fast while connected, `03` end the session |

### Files protocol

| Request | Answer notifications |
|---|---|
| `01` LIST | `81 seq:u16 "path,size\n"...` (packed up to the MTU), then `82 count:u16 crc32:u32` (CRC over all line bytes) |
| `02 offset:u32 path` READ | `83 offset:u32 data...`, then `84 status:u8 size:u32 crc32:u32` |
| `03 offset:u32` ACK | (flow control) |
| `04` ABORT | `84` with status aborted |
| bad request | `8F status:u8` |

- **Order:** the page sends one GATT operation at a time (Chrome rejects overlapping ones, e.g. the 5 s status poll during a READ write).
- **Paths** are relative to `/log`: `error.log`, `sync.csv` and `<run folder>/<file>` (all files of every run folder). Anything else is refused (bad path). Old flat files are not listed.
- **Status:** 0 ok, 1 bad request, 2 bad path, 3 not found, 4 busy, 5 SD error, 6 aborted.
- **CRC-32** (IEEE, as zlib) of the bytes `[offset, size)` sent by this READ; for offset 0 that is the file's CRC.
- **Flow control:** at most 8 KB beyond the last ACK in flight; the page ACKs every 4 KB. Without an ACK for 15 s the READ is aborted. Notifications that find NimBLE out of buffers are retried.
- **Resume:** READ from the number of bytes already stored. The size is fixed at the start of the READ. A file smaller than the stored copy (new card) is fetched again from 0 by the page.
- **MTU:** the device asks for 517; each data packet carries MTU − 8 bytes.
- **SD:** read in 4 KB blocks, each an open / seek / read / close in the App loop (never during a panel refresh, the bus is shared; the panel stays powered). Routine flushes wait while a READ runs; the rows stay in the RAM buffer (256 rows, over 20 min even in Fast) and are written right after. A nearly full buffer is still written.

### Build cost

- Flash: 2 019 127 B vs. 1 597 227 B without Bluetooth (+422 KB: NimBLE host and the controller library).
- IRAM: the controller needs ~33 KB IRAM, which overflowed `iram0_0_seg` by 3.9 KB next to Wi-Fi and PSRAM. `tools/iram_trim.py` (post script of `env:m5paper`) links against a patched copy of the framework's `sections.ld` in the build folder that puts the newlib time and stdio-buffer objects into flash instead of IRAM (the `SPIRAM_CACHE_LIBTIME/LIBIO_IN_IRAM` placements of the prebuilt libs; nothing calls them with the cache disabled). About 2.7 KB IRAM remain free. The script stops the build if the framework's script no longer matches.
- RAM: the stack is only allocated while Bluetooth is on. Bench: free heap 170 KB before, 143 KB advertising, 185 KB after `deinit` (about 27 KB while on).
- Transfer rate (bench, Chrome on Windows, USB power): 24.5 KB/s on one file, 6.7 KB/s on another session with small files (per-file overhead dominates). A 150 KB Eco day file takes about 6 s.

### The page (`web/`)

`index.html`, `app.js`, `ble.js` (protocol), `store.js` (IndexedDB cache), `sw.js` + `manifest.webmanifest` (installable offline app), `vendor/uPlot` 1.6.32 (MIT, local copy), icons. Plain HTML + JS, no external requests. `KalliFunghi.html` is the same page as one file, generated by `make_single.py` (do not edit it; rerun the script after every page change and commit both). The Pages workflow runs `make_single.py --check` and fails if the committed single file does not match the sources, so it cannot go stale unnoticed; run the check locally with `"C:\Program Files\KiCad\10.0\bin\python.exe" web\make_single.py --check`. `make_icons.py` draws the PNG icons. Web Bluetooth needs a secure context: HTTPS, `http://localhost` or a local file.

Three ways to open it:

1. **GitHub Pages (phones and PCs):** `.github/workflows/pages.yml` publishes `web/` on every push to `main` that changes it (it also checks that `KalliFunghi.html` is current). One-time setup by the repo owner: Settings > Pages > Source "GitHub Actions". The address is then `https://pilger15.github.io/KalliFunghi-M5paper/`. Open it once online, then "Install app" / "Add to home screen": the service worker keeps every file, so the app works fully offline and updates the next time it is online (network first with a 3 s timeout, then the cache).
2. **Local copy (no internet, no hosting):** **`web/KalliFunghi.html` is the file to copy to a phone or send by email** (one file, everything inlined). Open it in Chrome or Edge. Tested on Android: opened from a FairEmail attachment in Chrome, Bluetooth works. Do not send `index.html` alone: it needs its folder and shows "This page is incomplete" without it. Tested: Chrome 156 and Edge 156 on Windows from `file://` (secure context, `navigator.bluetooth` present, scripts and charts work, storage survives a browser restart); the user's Android phone with Chrome 156 from a `content://` email attachment (secure context, chooser works, IndexedDB survives reloads and restarts; Chrome crashed once during that test, cause unknown). The multi-file `index.html` also works from `file://` when the whole `web/` folder is copied, but a lone `index.html` cannot load its scripts. No service worker on a local file. The cache belongs to where the file is opened from: another path or app starts empty.
3. **localhost on the PC:** double-click `web/start-local.cmd`. It serves `web/` with KiCad's Python on `http://localhost:8765/` (fixed port: the cache is per origin, another port starts empty) and opens the browser.

If Bluetooth is missing, the page's banner names the reason (protocol, secure context, `navigator.bluetooth`, user agent) with a hint: plain http from a LAN address is not secure, in-app viewers and WebViews have no Web Bluetooth, Firefox and Safari have none.

## SD log

### Folder layout

Data is grouped in **measurement runs**, one per grow batch. Each run has its own folder; the plots show only the current run.

```
/log/error.log                      events and errors, all runs (utc_iso,clock,event)
/log/sync.csv                       clock syncs, all runs (utc_iso,correction_s)
/log/m004_2026-11-03/               run 4, started 3 Nov 2026 (local date)
    run.txt                         run info, see below
    2026-11-03.csv                  one CSV per UTC day
    2026-11-04.csv
    unsynced.csv                    rows logged before the first clock sync, if any
/log/m005_unsynced/                 a run started before the clock was ever synced
/log/2026-10-24.csv, unsynced.csv   flat files from firmware before 2026.11.03: left alone, not plotted
```

- **Folder name:** `m`, the run number with at least 3 digits, `_`, the start date in local time (`YYYY-MM-DD`), or `unsynced` if the clock had never been synced. A folder is never renamed; run 1000 is `m1000_...`. Any folder in `/log` matching `m<digits>_*` is a run; everything else (`error.log`, `sync.csv`, old flat files) is not.
- **Day files:** named by the UTC date, same CSV format as below.
- **`run.txt`:** plain text, one `key=value` per line. The first six lines are written when the run starts; later events are appended:

  ```
  run=4
  start_utc=2026-11-03T09:12:00Z
  clock=synced                            (unsynced: start_utc is on the unsynced clock)
  firmware=2026.11.03
  last_cal_utc=2026-10-28T08:00:00Z       (or never; calibration state at the start)
  temp_offset_c=4.5                       (SCD41 offset at the start)
  calibration=2026-11-05T10:30:00Z,+12    appended per successful CO2 calibration (ppm correction)
  clock_synced=2026-11-03T11:00:00Z,+3600 appended at the first sync of a run with unsynced rows (correction s)
  recreated_utc=2026-11-20T08:00:00Z      appended if the folder was recreated on another card
  ```

  Readers should ignore unknown keys.
- **Run state in NVS:** run number, folder name and start time survive reboots and firmware updates. On boot the firmware keeps writing into that folder; if it is missing (new or swapped card), it is created with a new `run.txt` from the NVS values plus `recreated_utc=`.
- **New run:** Settings, **New measurement**. The number is one above both the NVS run and the highest run folder on the card, so a swapped card never gets a number twice. The RAM buffer is written into the old run first; if that fails (card missing or full), the run is not switched. `error.log` gets `new measurement m005_2026-11-20`.
- **First boot with this firmware:** run 1 (or one above the highest on the card) starts automatically, as soon as a card is usable.

### CSV format

- One row per sample:

  ```
  utc_iso,co2_ppm,temp_c,rh_pct,lux,mode,clock
  2026-10-24T10:00:00Z,812,21.34,88.50,140.5,eco,ntp
  2026-10-24T10:00:30Z,,,,140.2,eco,ntp
  ```

- A missing sensor leaves its fields empty.
- `mode` is `ultra`, `eco` or `fast`.
- **Clock never synced:** rows go to `unsynced.csv` in the run folder with `clock=unsynced`, so they don't land in a wrong day file. Every sync appends `utc_iso,correction_s` to `/log/sync.csv`, so the unsynced rows can be shifted later. In RAM, the plot data is shifted automatically at the first sync.
- **Batched writes:** rows are buffered in RAM (256 rows) and written every 5 min together with the idle display refresh. They are also written before a mode change, a sync and a low-battery power-off.
- **Missing or full card:** the device keeps running and shows `! SD`. The card is mounted again at the next write. If the RAM buffer overflows, the oldest rows are dropped.
- **Long plot ranges:** 24 h and 7 d are built from the current run's day files. 1 h and 6 h come from RAM (PSRAM ring), which is cleared when a new run starts.

## Power

### Sampling modes

| | Ultra-Eco | Eco (default) | Fast |
|---|---|---|---|
| SCD41 | single shot every 5 min | low-power periodic, 30 s | periodic, 5 s |
| VEML7700 | every 5 min | every 30 s | every 5 s |
| Display, idle | every 5 min (GC16) | every 5 min (GC16) | every 5 min (GC16) |
| Display, interactive | every 30 s | every 30 s | every 5 s |

- **Interactive refreshes:** DU waveform, with a full GC16 refresh on every view change and every 10th refresh. A refresh is skipped if nothing on screen changed.
- **USB power:** Fast sampling and 5 s refreshes, with no timeout. When USB goes away, the device returns to your mode, in idle.
- **Low battery:** at 3.55 V (measured at rest, 3 readings in a row) the device shows "Battery low, last update <time>", writes the SD buffer and powers off. It starts again only at 3.65 V or more.

### Sleep strategy

- **Light sleep between wakes.** RAM and PSRAM are kept, and the GT911 touch interrupt (GPIO36) wakes the chip.
- **E-ink panel:** in standby between interactive refreshes, and powered off (GPIO23) between idle refreshes. Waking from off costs about 800 ms of re-init.
- **SD card:** written only while the panel is powered. SD and panel share the SPI bus, and an unpowered panel controller loads the bus.
- **USB-serial chip:** GPIO1 (U0TXD) is kept as an input, so it cannot power the unpowered chip.
- **Main power hold:** GPIO2 is held through resets, so a crash on battery does not switch the board off.

### Expected average current and battery life (estimate, not measured)

Assumptions:
- **SCD41** (datasheet, at 5 V): low-power periodic 2.8 mA, periodic 11 mA, single shot every 5 min 0.36 mA. Through the 5 V boost (~85 %) from a 3.8 V cell that is ~4.3 / 17 / 0.6 mA at the battery.
- **Board floor** (ESP32 light sleep 0.8 mA, GT911 in its touch-detect mode, PSRAM, regulators, boost, SD standby): 2.5 to 5 mA, not measured yet.
- **Events:** a wake about 60 ms at 40 mA, GC16 with re-init about 150 mAs, DU from standby about 30 mAs.

| Mode | Average current | Battery life (1150 mAh) |
|---|---|---|
| Ultra-Eco, idle | 3.5 to 6 mA | about 8 to 14 days |
| Eco, idle | 7.5 to 10 mA | about 5 to 6 days |
| Fast, idle (and USB) | 21 to 23 mA | about 2 days |

Interactive use adds about 1 mA (Eco and Ultra, 30 s refresh) or 6 mA (Fast, 5 s refresh) while it lasts. The SCD41 dominates in Eco and Fast; the board floor dominates in Ultra-Eco. Replace these numbers with bench measurements (see Debug).

### USB detection

The M5Paper has no VBUS or charge-status pin. `config.h` selects the method:
- **`UsbMethod::BatteryHeuristic`** (current default). One voltage point per minute, 10 min window:
  - **USB in:** a rise of 30 mV over the window (charging), or a flat level of 4.15 V or more, but the flat rule applies only right after boot.
  - **USB out:** a drop of 20 mV (two points in a row), or the voltage crossing below 4.10 V.
- **Detection delays:**
  - Plug-in: about 10 min.
  - Unplug: about 2 min when the voltage steps down. A full cell with a small step is only caught at the 4.10 V crossing, which can take hours at Fast drain.
  - Re-plugging a nearly full cell (above about 4.17 V) may go unnoticed.
- **`UsbMethod::Gpio3`:** reads the USB-serial chip's TX line (GPIO3, pull-down). It is only trustworthy after the bench run (`bench_usb`, `bench_usb_sleep`, procedure in `bench/usb_detect/main.cpp`).
- **Hardware option (not fitted):** a 100k/100k divider from VBUS at the USB-C connector to a free ADC1 pin. Unambiguous, and draws 25 uA only while USB is present.

## Debug

`pio run -e m5paper_debug -t upload`. The trace goes to **Serial2 TX on Grove port C (GPIO18), 115200 baud**. Connect a 3.3 V USB-UART adapter (GND and RX only), so the board runs from its battery or a bench supply on the battery connector without USB. UART0 is not used because its pins belong to USB detection.

Per wake it prints the awake time, the planned sleep, sample count, GC16 and DU counts, power state and battery mV. It also prints boot, mode, sync, calibration and SD events.

## Code layout

```
lib/kfcore/src/   pure logic, no Arduino (unit tested)
  config.h          pins, timings, thresholds
  PowerStateMachine power states, sampling mode, refresh interval (diagram in the header)
  TapClassifier     tap vs double tap
  UsbHeuristic      USB from battery voltage
  Layout            screen geometry and hit-testing
  ColumnBins        min/max per pixel column
  SampleRing        RAM ring of samples
  RunLog            run folder names and paths, run.txt
  BleProto          Bluetooth byte formats, CRC-32, flow-control window
  CsvFormat, TimeUtil, CalReminder, BatteryMonitor, Types
src/
  App               tick loop, wiring
  hw/               power hold, light sleep, debug trace
  power/UsbDetect   heuristic or GPIO3
  sensors/          SCD41, VEML7700
  storage/          SD (log, config, history), NVS settings
  time/             BM8563 clock, Wi-Fi + NTP sync
  ble/BleSync       Bluetooth session: GATT server, file transfer
  ui/               e-ink power and waveforms, rendering
bench/usb_detect/   bench sketch
tools/iram_trim.py  linker script patch for the Bluetooth IRAM overflow
test/test_core/     unit tests
```

## Not verified without hardware

Everything here builds, and the unit tests compile and link for the ESP32. Nothing has run on the M5Paper yet. Open points:
- **Whole program:** first boot, rendering, layout and fonts on the real panel.
- **Touch:** whether a short tap reliably wakes the chip from light sleep, and whether it registers. The GT911 interrupt is a pulse, and short taps may be released before the first read.
- **Panel power:** whether re-init after GPIO23 off works repeatedly. SD access right after a panel re-init (same SPI bus).
- **Power figures:** the GT911 current in light sleep; panel standby vs off current; leakage with GPIO23 low; SD idle current; all battery-life numbers.
- **Crash on battery:** whether the GPIO2 hold keeps the board on through a reset.
- **USB detection:** GPIO3 behavior, and the voltage heuristic thresholds on this cell.
- **SCD41:** single shot without the library's blocking call, the forced recalibration, and the self-heating temperature offset.
- **VEML7700:** the reading after 130 ms of light sleep.
- **Wi-Fi sync:** against a real hotspot, including the disconnect reason for a wrong password.
- **Timing:** DU vs GC16 timing on this panel.
- **Unit tests:** not run yet. This machine has no host compiler; run them on the device with `pio test -e m5paper`, or install gcc.
