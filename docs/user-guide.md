<!-- Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Low | Reviewed by: None -->
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

The home screen shows the last 6 h of each value (change the range in Settings): the current value large, plus the lowest and highest value in that time. The sidebar always shows the time and date, battery, mode and how long ago the clock was synced. On the home screen it also shows how long ago the CO2 sensor was calibrated ("CO2 cal 12d", or "never") and the current measurement with its day ("Run 4, day 12"). Marks appear there only when something needs attention (see Troubleshooting).

## Touch controls

| Gesture | What happens |
| --- | --- |
| Any touch | The screen updates more often for 5 min. Each further touch restarts the 5 min. |
| Tap a plot | Shows that plot full screen. |
| Double tap anywhere | Back to the home screen, from any page. |
| Tap **Sync** | Connects to your Wi-Fi network (see Wi-Fi) or the phone hotspot and sets the clock. |
| Tap **Settings** | Opens the settings; tap it again to leave. |

Buttons react after about half a second: the device first waits to see whether a second tap follows. E-ink needs another moment to redraw, so wait for the screen to change before tapping again.

On the keyboard this is different: keys react at once, and a double tap does not go home (passwords have double letters). Leave the keyboard with **Cancel**.

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

If you saved a Wi-Fi network (see Wi-Fi), Sync tries it first and then the phone hotspot.

**Auto-sync:** once a day, if the last sync is older than 7 days, the device quietly tries your saved network and then the hotspot, 10 s each. Leaving the hotspot on near the device is enough.

"! Clock" in the sidebar means the clock was never synced or the last sync is older than 30 days. Readings taken before the first sync are still saved, in a separate file.

**Using a different hotspot:** put a file named `config.json` in the top folder of the SD card with your hotspot's name and password, then restart the device:

```json
{ "wifi": { "ssid": "your-hotspot", "password": "your-password" } }
```

## Wi-Fi

In Settings, tap **Wi-Fi...**. The page shows which networks Sync uses.

1. Tap **Scan**. After a few seconds the networks nearby appear, strongest first, with signal bars and a lock for password-protected ones. **Up** and **Down** page through a long list.
2. Tap your network. For a protected network the keyboard opens: type the password and tap **OK**. Open networks connect right away.
3. The device tries to connect and shows the result: "Connected ..., saved", "Wrong password", "Network not found" or "Connect timeout". Only a network that connected is saved.
4. Tap **Sync now** to set the clock over it.

**Hidden network:** tap **Enter name**, type the network name, **OK**, then the password (leave it empty for an open network).

**Forget** removes the saved network; Sync then only uses the phone hotspot. One network can be saved. Networks that need a user name (802.1X, eduroam) are not supported.

**Keyboard:** **Shift** gives one capital letter; tap it twice quickly for Caps Lock, once more to turn it off. **123** shows digits and symbols, **#+=** more symbols, **abc** goes back to letters. **Del** deletes the last character. **Hide** hides the password as `*` (it is shown by default). The counter shows the length; **OK** only works with 8 to 63 characters (network names: 1 to 32). The keyboard closes by itself after 3 min without a key. While typing, the screen redraws only the text, with a faster mode that can leave faint traces; they clear when you pause.

The password is stored on the device without encryption.

## Settings

Tap **Settings** in the sidebar; tap it again or double tap to leave. Settings are kept when the device is switched off.

| Setting | Options | Default |
| --- | --- | --- |
| Plot range | 1 h, 6 h, 24 h, 7 d | 6 h |
| Sampling mode | Ultra-Eco, Eco, Fast | Eco |
| Auto-sync | On, Off | On |
| Temperature offset | 0 to 10 °C in 0.5 steps | 0 °C |
| CO2 calibration | Calibrate CO2 now | — |
| Measurement | New measurement | — |
| Wi-Fi | Scan, choose, forget (see Wi-Fi) | phone hotspot |

**CO2 calibration:** the device corrects its CO2 reading against fresh outdoor air (420 ppm). Once a week a box at the bottom of the screen reminds you; the device keeps working normally while it is shown.

1. Place the device outdoors for 30 min, away from people, traffic and exhaust.
2. Tap **Calibrated**. Leave it outside until the result appears; sometimes it first counts down up to 3 min.
3. The box shows the correction and disappears. **Postpone** hides the reminder for 24 h.

You can also calibrate at any time with **Calibrate CO2 now** in Settings: same steps, and the weekly reminder starts again from that day. Settings shows the date of the last calibration.

**New measurement:** tap it when you start a new grow batch. After you confirm, the device starts a new run: the plots start empty and the data goes into a new folder on the SD card. Earlier data stays on the card. The device then offers the CO2 calibration (**Yes** or **Later**); right before a new batch is a good time for it. If the SD card is missing or full, no new run is started.

**Temperature offset:** the device warms itself slightly, so temperature can read a little high and humidity a little low. After 30 min next to a trusted thermometer, set the difference as the offset (shown temperature minus thermometer). The offset can only lower the reading.

## Data and battery

**Data:** every reading goes to the SD card, into one folder per measurement in `/log/`, for example `/log/m004_2026-11-03/` for run 4, started on 3 Nov 2026. Inside is one file per day, for example `2026-11-03.csv`; open it in Excel or any spreadsheet. Times are in UTC (local time minus 1 h in winter, minus 2 h in summer). Readings from before the first sync are in `unsynced.csv` in the same folder. `run.txt` lists the run's start, the calibration state and offset at the start, and each calibration. Files directly in `/log/` with a date name come from older firmware.

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
| "Network not found" | Check that the network or hotspot is on, uses exactly the name set up for the device, and runs on 2.4 GHz ("Maximize Compatibility" on iPhone). Move it closer. |
| "Wrong password" | Check the password: on the Wi-Fi page choose the network again, or for the hotspot the password set up for the device. |
| "Connect timeout" | The network did not answer in time. Move closer and try again. |
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
