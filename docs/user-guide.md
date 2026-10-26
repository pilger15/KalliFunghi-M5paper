<!-- Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Low | Reviewed by: None -->
<!-- Exported from the Claude Docs "KalliFunghi User Guide" (https://claude.ai/artifact/4k9WHv2QxgfjobWTJo262V). Edit there, then export again. -->

# KalliFunghi User Guide

Oct 26, 2026 · Julian

KalliFunghi logs CO2, temperature, humidity and light in your grow box, shows the recent hours on its e-ink screen and saves every reading to the SD card. In the default mode it runs about 5 to 6 days on one battery charge.

## Getting started

1. Insert the SD card and plug the sensor cable into **Port A**.
2. Switch the device on: press the side button for about 2 s. The screen flashes black and white a few times, then shows the home screen.
3. Turn on your phone's internet hotspot with the hotspot name and password set up for this device.
4. Tap **Sync** in the sidebar. The clock is set when the bottom of the screen shows "Sync ok".
5. Place the device in the grow box, away from lamps and heaters. The first values appear after about a minute.

## The screen

Four plots on the left (CO2, temperature, humidity, light), the sidebar with status and the **Sync** and **Settings** buttons on the right. The online version of this guide shows a picture of the layout.

The home screen shows the last 6 h of each value (change the range in Settings): the current value large, plus the lowest and highest value in that time. The sidebar always shows the time and date, battery, mode and how long ago the clock was synced. Marks appear there only when something needs attention (see Troubleshooting).

## Touch controls

| Gesture | What happens |
| --- | --- |
| Any touch | The screen updates more often for 5 min. Each further touch restarts the 5 min. |
| Tap a plot | Shows that plot full screen. |
| Double tap anywhere | Back to the home screen, from any page. |
| Tap **Sync** | Connects to the phone hotspot and sets the clock. |
| Tap **Settings** | Opens the settings; tap it again to leave. |

Buttons react after about half a second: the device first waits to see whether a second tap follows. E-ink needs another moment to redraw, so wait for the screen to change before tapping again.

## Modes and update rates

Use **Eco** for normal operation. Choose **Ultra-Eco** when the device should run as long as possible on battery, and **Fast** only when you need to watch quick changes.

| Mode | Readings every | Screen, untouched | Screen after a touch (5 min) | Battery per charge |
| --- | --- | --- | --- | --- |
| Ultra-Eco | 5 min | 5 min | 30 s | about 1 to 2 weeks |
| Eco (default) | 30 s | 5 min | 30 s | about 5 to 6 days |
| Fast | 5 s | 5 min | 5 s | about 2 days |
| On USB power | 5 s | 5 s | 5 s | charging |

Battery times are first estimates and will be updated after measurements.

On USB power the device switches to full speed by itself and shows a plug symbol in the sidebar. It can take up to 10 min until it notices the cable. Unplugged, it returns to your chosen mode; when the battery is full this can take longer.

## Syncing the clock

Sync about once a month; the clock drifts roughly 1 min per month. Summer and winter time switch automatically.

1. Turn on your phone's internet hotspot with the hotspot name and password set up for this device. The device only uses 2.4 GHz: on an iPhone, turn on "Maximize Compatibility".
2. Hold the phone within a few meters of the device.
3. Tap **Sync**. It takes up to 20 s.
4. The bottom of the screen shows "Sync ok" and how many seconds the clock was corrected, or why it failed.
5. Turn the hotspot off again.

**Auto-sync:** once a day, if the last sync is older than 7 days, the device quietly looks for the hotspot for 10 s. Leaving the hotspot on near the device is enough.

"! Clock" in the sidebar means the clock was never synced or the last sync is older than 30 days. Readings taken before the first sync are still saved, in a separate file.

**Using a different hotspot:** put a file named `config.json` in the top folder of the SD card with your hotspot's name and password, then restart the device:

```json
{ "wifi": { "ssid": "your-hotspot", "password": "your-password" } }
```

## Settings

Tap **Settings** in the sidebar; tap it again or double tap to leave. Settings are kept when the device is switched off.

| Setting | Options | Default |
| --- | --- | --- |
| Plot range | 1 h, 6 h, 24 h, 7 d | 6 h |
| Sampling mode | Ultra-Eco, Eco, Fast | Eco |
| Auto-sync | On, Off | On |
| Temperature offset | 0 to 10 °C in 0.5 steps | 0 °C |
| CO2 calibration | Calibrate now (new batch) | — |

**CO2 calibration:** the device corrects its CO2 reading against fresh outdoor air (420 ppm). Once a week a box at the bottom of the screen reminds you; the device keeps working normally while it is shown.

1. Place the device outdoors for 30 min, away from people, traffic and exhaust.
2. Tap **Calibrated**. Leave it outside until the result appears; sometimes it first counts down up to 3 min.
3. The box shows the correction and disappears. **Postpone** hides the reminder for 24 h.

Before a new batch, use **Calibrate now (new batch)** in Settings: same steps, and the weekly reminder starts again from that day.

**Temperature offset:** the device warms itself slightly, so temperature can read a little high and humidity a little low. After 30 min next to a trusted thermometer, set the difference as the offset (shown temperature minus thermometer). The offset can only lower the reading.

## Data and battery

**Data:** every reading goes to the SD card, one file per day in `/log/`, for example `2026-10-26.csv`. Open it in Excel or any spreadsheet. Times are in UTC (local time minus 1 h in winter, minus 2 h in summer). Readings from before the first sync are in `/log/unsynced.csv`.

| Column | Meaning |
| --- | --- |
| utc_iso | Date and time of the reading (UTC) |
| co2_ppm | CO2 in ppm |
| temp_c | Temperature in °C |
| rh_pct | Relative humidity in % |
| lux | Light in lux |
| mode | ultra, eco or fast |
| clock | ntp = clock was synced, unsynced = not yet |

An empty field means that sensor was not connected at that moment.

To copy the data, switch the device off first, then take out the SD card. Readings are written every 5 min, so the last few minutes before switching off may be missing.

**Battery:** charge with any USB-C charger. When the battery is nearly empty, the device saves its data, shows "Battery low" with the time of the last update and switches off. Charge it, then press the side button to start again.

## Troubleshooting

| Symptom | What to do |
| --- | --- |
| "Hotspot not found" | Check that the hotspot is on, uses exactly the name set up for the device, and runs on 2.4 GHz ("Maximize Compatibility" on iPhone). Hold the phone closer. |
| "Wrong password" | Check the hotspot password set up for the device. |
| "NTP timeout" | The hotspot has no internet. Check mobile data on the phone. |
| "! Clock" in the sidebar | Sync the clock. |
| "no sensor" in a plot | Check the cable in Port A, then switch the device off and on. |
| "! SD" in the sidebar | SD card missing, full or damaged. The device keeps measuring and saves again once a working card is in. |
| "Cal due" in the sidebar | CO2 calibration is due, see Settings. |
| CO2 reads far too high or low | Calibrate outdoors (see Settings). |
| Temperature reads high | Set the temperature offset (see Settings). |
| No plug symbol on USB | Normal for up to 10 min after plugging in. |
| Faint ghost images on screen | Switch the device off and on; it cleans the screen at every start. |
| Screen does not react | Wait until the screen has finished changing, then tap again. |
