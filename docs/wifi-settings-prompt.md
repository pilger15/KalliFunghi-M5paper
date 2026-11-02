<!-- Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None -->

# KalliFunghi: Wi-Fi selection and on-screen keyboard (prompt for a separate chat)

Recommended: **Opus 5.5, effort high.** Start in plan mode in `C:\dev\personal\KalliFunghi`. Start it after the SD log fix (session "Investigate missing SD log files") is committed, because both touch `App.cpp`, `Ui.cpp` and `Display.cpp`.

---

## Prompt

Add manual Wi-Fi selection with an on-screen keyboard to the KalliFunghi firmware (`firmware/`, PlatformIO, M5Paper with M5GFX). Read `docs/firmware-prompt.md` (original spec), `docs/firmware-plan.md`, `firmware/README.md` and the existing code first, mainly `src/ui/Ui.*`, `src/ui/Display.*`, `src/App.cpp`, `src/time/TimeSync.*`, `src/storage/Settings.*`, `lib/kfcore/src/TapClassifier.*` and `lib/kfcore/src/Layout.*`. Start in plan mode: propose the screens, the state changes and the files you will touch, and wait for my OK.

### What exists today
Connect & Sync uses one network: the phone hotspot from `secrets.h` (git-ignored), overridable by `/config.json` on the SD card. Settings are stored in NVS (`Settings`). Double tap anywhere returns to home; `TapClassifier` holds back a single tap for the double-tap window.

### New: Wi-Fi page
- Settings gets a **Wi-Fi** button that opens a Wi-Fi page.
- The page shows the network in use for sync and a **Scan** button.
- Scan: Wi-Fi on, scan, Wi-Fi fully off again (same power rules as the sync). Show the found networks sorted by signal strength: SSID, signal bars, lock mark if secured. Merge duplicate SSIDs. Page through the list if it does not fit (big rows, at least 64 px tall).
- Tap a network: open networks go straight to the connection test; secured ones open the keyboard for the password.
- **Hidden network:** a button "Enter name" opens the keyboard for the SSID, then for the password.
- **Connection test** after entering the password: connect, show the result (connected, wrong password, not found, timeout), then Wi-Fi off. Only on success save the network and offer **Sync now**.
- **Forget** removes the saved network.
- Store one user-selected network (SSID and password) in NVS. Sync and auto-sync try it first, then fall back to the default hotspot. Precedence of the fallback stays: `/config.json` over `secrets.h`. Auto-sync limits each attempt to 10 s as before.
- The password is stored in plain text in NVS (no flash encryption). Note this in the README.

### New: on-screen keyboard
- Landscape QWERTY across the full screen (960x540). The sidebar is hidden while it is open. Keys at least 72x64 px.
- Layers: lowercase, uppercase (Shift, one-shot; double Shift = Caps Lock), numbers and symbols (all printable ASCII, so any WPA password can be typed). Keys: Backspace, Space, Cancel, OK.
- A text field at the top with the text and the cursor at the end, a **Show/Hide** toggle for the password (default: shown, it's a private device), and a length counter. WPA2 passwords: 8 to 63 characters; OK is disabled outside that range. SSIDs: 1 to 32 bytes.
- **Gestures change on this page:** taps fire immediately (no waiting for a double tap) and double tap does **not** go home, since passwords contain double letters. Cancel is the only way out. Make this a property of the view so other text-entry pages behave the same.
- Inactivity timeout 3 min: leave like Cancel.

### Keyboard mode: maximum refresh speed
- Entering the keyboard: one full GC16 refresh. Leaving: one full GC16 refresh of the next page.
- While typing: use the fastest waveform M5GFX offers (`epd_fastest`, A2 if that is what it maps to; check) and refresh **only the changed region** (the text field, or the keyboard area on a layer switch), not the whole screen. Check that M5GFX's IT8951 panel really pushes only the dirty rectangle; if not, push the region explicitly.
- Keep the panel awake (no standby or power-off), no light sleep, and poll the touch controller at full rate while the keyboard is open. Sensor sampling and logging continue in the background but must never block a key press; defer SD writes until the keyboard closes unless the buffer is full.
- Key feedback: decide whether a short key highlight is worth a second refresh per key; measure and tell me the time from touch to visible character, with and without it. Target: under 300 ms.
- A2 ghosts: redraw the text field with DU or GC16 every N keystrokes or after a pause of about 1 s; pick N by testing.
- Keyboard mode counts as interactive for the power state machine and resets the 5 min timer on every key.

### Deliverables
- Wi-Fi page, keyboard, NVS storage and sync fallback, built and flashed (`--upload-port COM10`, `~/.platformio/penv/Scripts/pio.exe`). No host gcc: run unit tests on the device (`pio test -e m5paper`).
- Unit tests for the testable logic: keyboard layout and hit-testing, text editing rules (length limits, Shift/Caps), network list sorting and de-duplication, sync fallback order.
- Update `docs/user-guide.md` and `firmware/README.md` with the Wi-Fi page and the keyboard.
- Follow my global CLAUDE.md code-tagging rules. Display refresh, touch timing, Wi-Fi power and NVS code are review priority High.
- List what you could not verify on the hardware.
