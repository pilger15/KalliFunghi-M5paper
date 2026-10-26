<!-- Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None -->

# KalliFunghi firmware: prompt for a separate chat

Recommended: **Opus 5.5, effort high.** Start the chat in plan mode in `C:\dev\personal\KalliFunghi`.

---

## Prompt

You are writing the firmware for **KalliFunghi**, a battery-powered environment logger for a mushroom grow box. Work in `C:\dev\personal\KalliFunghi\firmware` as a PlatformIO project (Arduino framework, ESP32). Start in plan mode: read this spec, check the libraries, propose the architecture and file layout, and wait for my OK before writing code.

### Hardware
- **M5Stack M5Paper** (ESP32-D0WDQ6-V3, 8 MB PSRAM, 4.7" e-ink 960x540 with IT8951 controller, GT911 touch, BM8563 RTC, SD card, 1150 mAh LiPo). The SD card and the IT8951 share the SPI bus.
- **Sensirion SCD41** (CO2, temperature, humidity) and **Vishay VEML7700** (ambient light), both on I2C via Grove port A.
- Use M5Unified + M5GFX if they support the M5Paper EPD refresh modes and touch properly; otherwise M5EPD. Check before deciding, and tell me which one and why. Use the Sensirion SCD4x library and a maintained VEML7700 library.

### Goal
Lowest possible battery consumption. Battery is the normal power source; USB is occasional.

### Sampling modes (user setting, default Eco)
| | Eco (default) | Fast |
|---|---|---|
| SCD41 | low-power periodic, 30 s | periodic, 5 s |
| VEML7700 | every 30 s (same tick) | every 5 s |
| Display, idle | every 5 min | every 5 min |
| Display, interactive | every 30 s | every 5 s |

- **Interactive mode**: any touch enters it. It lasts 5 min, and every touch restarts the 5 min timer. Then back to idle.
- Switching modes needs the SCD41 stop/start sequence from the datasheet (stop_periodic_measurement, wait 500 ms, start the other mode). Discard the first reading after a mode change.
- **Disable SCD41 automatic self-calibration (ASC).** A grow box rarely sees 400 ppm, so ASC would drift the readings. Offer a manual forced recalibration in settings (instruction on screen: "Place outdoors for 5 min, then press", target 420 ppm).
- Set the SCD41 temperature offset (default 0, adjustable in settings later). The M5Paper back gets warm; note this in the README.

### USB power: automatic performance mode
- While USB power is present, run **Fast sampling with interactive refresh (5 s) continuously**, no 5 min timeout. When USB goes away, fall back to the user's chosen mode (idle). Show a plug mark in the sidebar.
- The M5Paper has no PMIC and, as far as the M5Stack docs show, no VBUS or charge-status pin. Before coding the detection, write a small bench sketch that logs, with and without USB: battery voltage (ADC on GPIO35, check against the M5 library), the level of GPIO3 (U0RXD, driven by the CP2104; if the CP2104 is powered only from USB, its TXD idles high with USB and is low or floating without it), and any other candidate. I run it and report back.
- Preferred detection, depending on the bench result: (1) GPIO3 level, if it is reliable; (2) otherwise a battery-voltage heuristic: USB if the voltage rises at least 30 mV over 10 min, or stays at 4.15 V or above; battery if it falls below 4.10 V or drops steadily. Document the unplug detection delay of the heuristic.
- Check USB state on every tick; it is a cheap read.
- Fallback if neither works: a hardware option (voltage divider from VBUS at the USB-C connector to a free ADC pin). Only propose it, I decide.

### Sleep strategy
- Prefer **ESP32 light sleep** between ticks: it keeps RAM (plot buffers survive), and the GT911 interrupt (GPIO36) can wake it, so tap and double-tap detection work. Only move to deep sleep if measurements show light sleep is clearly worse; explain the trade-off first.
- Batch SD writes: buffer samples in RAM and write them together with the 5 min idle refresh (or when the buffer is full, or before a mode change or sync). Power the SD card only for the write if the board allows it.
- Turn the e-ink panel power off between refreshes.
- Provide a debug build flag that logs wake time, awake duration and tick counts over serial, so I can measure current with a bench supply. Give me an expected average current per mode, with assumptions.
- In the same debug build, toggle a free Port B/C GPIO high while awake and a second one during e-ink refresh, SD write and Wi-Fi, so a power profiler (Nordic PPK2 logic inputs) can mark each phase in the current trace. USB is unplugged during these measurements, so also log the phases to the SD card.
- Log battery voltage in every CSV row (`vbat_v` column) for long-term runtime checks.

### Time and sync
- The BM8563 RTC holds **UTC**. On every wake, read time from the BM8563 (the ESP32 sleep clock drifts too much). Local time via TZ `CET-1CEST,M3.5.0,M10.5.0/3`.
- **Connect & Sync** button in the sidebar: connect to the user's phone hotspot (SSID and password compiled in from `firmware/lib/kfcore/src/secrets.h`, which is not in git, overridable by `config.json` on the SD card), get NTP time (`pool.ntp.org`, fallback `ptbtime1.ptb.de`), write the BM8563, disconnect, turn Wi-Fi off. Timeout 20 s. Show the result on screen (success with correction in seconds, or the failure reason: hotspot not found, wrong password, NTP timeout).
- **Auto-sync** (setting, default on): once per day, if the last sync is older than 7 days, try the hotspot silently for at most 10 s.
- Show the last sync age in the sidebar; show a warning mark if the clock was never synced or the last sync is older than 30 days.
- Store the last sync time and settings in NVS.

### Screen layout (960x540, landscape)
- **Right sidebar** (about 130 px): local time and date, battery percent, mode (Eco / Fast, plus a marker when interactive), last sync age, buttons **Sync** and **Settings**.
- **Home**: 2x2 grid of plots in the remaining area: CO2 (ppm), temperature (degC), humidity (%RH), light (lux). Each plot shows the current value large, plus min/max over the range, axis labels and time ticks.
- **Tap a plot**: that plot goes full screen (sidebar stays).
- **Double tap anywhere**: always back to home. Pick a double-tap window (about 400 ms) and make sure a single tap on a plot does not fire before the window has passed only when needed; describe how you resolve tap vs. double tap.
- **Settings** page: plot range (1 h, **6 h default**, 24 h, 7 d), sampling mode (Eco / Fast), auto-sync on/off, SCD41 forced recalibration, SCD41 temperature offset. Big touch targets.
- E-ink refresh: fast partial mode (DU) for interactive updates, a full GC16 refresh on every view change and every 10th partial update to clear ghosting. Idle 5 min refreshes use GC16.
- Plots longer than the pixel width: downsample to min/max per pixel column. Changing the range to 24 h or 7 d rebuilds the plot from the SD files; keep 6 h in RAM (PSRAM).

### SD logging
- One CSV per day in UTC: `/log/YYYY-MM-DD.csv`, header `utc_iso,co2_ppm,temp_c,rh_pct,lux,vbat_v,mode`. One row per sample.
- If the clock was never synced, still log, but mark rows (for example `utc_iso` prefixed with `unsynced:` or a separate column; propose one) so they can be fixed later.
- Survive a missing or full SD card: keep running, show a mark in the sidebar.

### Robustness
- Sensor missing or I2C error: show "no sensor" in that plot, keep logging the other sensor.
- Battery low (below about 5%): one final screen "Battery low, last update <time>", then the lowest-power sleep.

### Deliverables
- PlatformIO project that builds; README with build, flash, `config.json` format and expected battery life per mode.
- Small, testable modules (sensors, power/sleep state machine, display/UI, logging, time sync, settings). Put the power state machine in one place and document its states and transitions in a comment or diagram.
- Follow my global CLAUDE.md code-tagging rules. Sleep, interrupts, timing, SPI sharing and RTC code are review priority High.
- List what you could not verify without hardware.
