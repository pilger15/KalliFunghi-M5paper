<!-- Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None -->

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
  - buttons **Sync** and **Settings**
- **Settings:** plot range (1 h, 6 h, 24 h, 7 d), sampling mode, auto-sync on/off, **Wi-Fi...** (see Wi-Fi page), SCD41 temperature offset (0 to 10 degC in 0.5 steps), **Calibrate CO2 now** (with the date of the last calibration), **New measurement** (with the current run and its start date).
- **Interactive mode:** any touch starts it for 5 min; every touch restarts the timer.

### Tap vs double tap

The double-tap window is 400 ms.
- A tap on something that does nothing on a single tap (the full-screen plot body, empty space) is not delayed. It only counts as the first half of a possible double tap.
- A tap on a button or a home plot waits until the 400 ms window has passed. A second tap inside the window means "home", and the single-tap action is never run.

The chip stays awake during the window (no light sleep), because M5Unified's light sleep drops touch data.

These rules are a property of the view (`layout::traits`). Text-entry views (the keyboard) act on touch-down, have no double-tap window, and a double tap does not go home; Cancel is the only way out.

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

- **Sync** tries the network saved on the Wi-Fi page first, then the default hotspot (`/config.json` over `secrets.h`). The radio stays on between the two attempts. It gets NTP time (`pool.ntp.org`, fallback `ptbtime1.ptb.de`), writes the BM8563 (UTC), then turns Wi-Fi off. Each attempt (connect plus NTP) is limited to 20 s.
- The result is shown at the bottom: "Sync ok via <network>" with the correction in seconds, or the reason of the last attempt: "Network not found", "Wrong password", "Connect timeout", "NTP timeout" or "Wi-Fi connect failed". Each attempt is one line in `error.log`.
- **Auto-sync** (default on): 5 min after boot and then once a day, if the last sync is more than 7 days old, it tries silently, 10 s per network.
- No sync below 3.7 V on battery, and none during a calibration.

### config.json

Optional, in the SD card root. It overrides the compiled-in default hotspot (the saved Wi-Fi page network is still tried first):

```json
{
  "wifi": {
    "ssid": "your-hotspot",
    "password": "your-password"
  }
}
```

## Wi-Fi page and keyboard

- **Wi-Fi page** (Settings, **Wi-Fi...**): shows the networks sync uses, **Scan**, **Enter name** (hidden network), **Forget**, **Up** / **Down** for long lists. The sidebar stays visible; double tap goes home.
- **Scan:** same rules as a sync (no Wi-Fi below 3.7 V on battery or while calibrating, NVS marker and `error.log` line before the radio starts), async scan for at most 10 s, then Wi-Fi fully off. Equal SSIDs are merged (strongest wins), hidden entries dropped, sorted by signal. Rows are 72 px, 6 per page; signal 0-4 bars, lock = WPA/WEP password, `802.1X` = enterprise (not supported).
- **Tap a network:** open networks go straight to the connection test, secured ones open the keyboard. **Enter name:** keyboard for the SSID, then for the password (empty = open network).
- **Connection test:** connect for at most 15 s, show "Connected ..., saved", "Wrong password", "Network not found", "Connect timeout" or "Wi-Fi connect failed", then Wi-Fi off. Only a successful network is saved; then **Sync now** appears.
- **Storage:** one network in NVS (namespace `kf`, keys `wSsid`, `wPass`), written only when a test succeeds or on Forget. **The password is stored in plain text** (no NVS or flash encryption on this board); anyone with the device and a USB cable can read it.

### Keyboard

- Full screen 960x540 QWERTY, sidebar hidden. Text field on top (prompt, length counter, text with the cursor at the end, **Show/Hide**, default shown), four key rows of 106 px (keys at least 96x106 px).
- Layers: lowercase, uppercase (Shift = one-shot; a second Shift within 400 ms = Caps Lock), symbols 1 and 2 (**123**, then **#+=**). Together all printable ASCII. Keys **Del**, **space**, **Cancel**, **OK**.
- Limits: passwords 8 to 63 characters, SSIDs 1 to 32 bytes. **OK** is drawn disabled (thin border, dotted line) outside the range; further characters are ignored.
- 3 min without a key: closes like Cancel.
- **Refresh:** entering and leaving is one full GC16. Each key redraws only the changed region (text field; key area on a layer or Shift change; the OK key when it changes state) with `epd_fastest`, which M5GFX 0.2.34 maps to **DU4** (A2 is only reachable through a private M5GFX method). M5GFX's IT8951 driver refreshes the bounding box of what was drawn since the last `display()`, so only that region is pushed. The push does not wait for the waveform; while the panel is busy, further keys are collected and pushed together as soon as it is free. Ghost cleanup: the text field (and the keys, if they changed) are redrawn with GC16 after 8 DU4 updates or after a 1 s pause.
- **Power:** while the keyboard is open, no light sleep and no panel standby; touch is polled every 2 ms, the rest of the tick (battery, RTC, sampling) runs once per second. The VEML7700 integration becomes a deadline instead of a 130 ms nap; SCD41 reinit, sensor mode changes, calibration and auto-sync wait until the keyboard closes. SD writes wait too, unless a buffer is nearly full (the queue is flushed when the keyboard opens). Every key restarts the 5 min interactive timer.

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
- **E-ink panel:** in standby between interactive refreshes, and in TCON sleep between idle refreshes. GPIO23 (panel power) stays on: switching it back on browns out the ESP32.
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
  Layout            screen geometry and hit-testing, view traits (gesture rules)
  Keyboard          on-screen keyboard keys, layers and text rules
  WifiList          scan list merge/sort, signal bars, sync network order
  WifiLayout        Wi-Fi page geometry and hit-testing
  ColumnBins        min/max per pixel column
  SampleRing        RAM ring of samples
  RunLog            run folder names and paths, run.txt
  CsvFormat, TimeUtil, CalReminder, BatteryMonitor, Types
src/
  App               tick loop, wiring
  AppWifi           Wi-Fi page and keyboard controller
  hw/               power hold, light sleep, debug trace
  net/WifiLink      Wi-Fi scan, connect, radio off
  power/UsbDetect   heuristic or GPIO3
  sensors/          SCD41, VEML7700
  storage/          SD (log, config, history), NVS settings
  time/             BM8563 clock, Wi-Fi + NTP sync
  ui/               e-ink power and waveforms, rendering (WifiView, KeyboardView)
bench/usb_detect/   bench sketch
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
- **Wi-Fi page and keyboard:** built, not run on the board yet: scan, connection test results, sync fallback, DU4 region refresh (no artifacts with overlapping updates?), touch-to-character time (target under 300 ms), ghost cleanup interval, key highlight (off by default, `kKbKeyHighlight`).
- **Unit tests:** not run yet. This machine has no host compiler; run them on the device with `pio test -e m5paper`, or install gcc.
