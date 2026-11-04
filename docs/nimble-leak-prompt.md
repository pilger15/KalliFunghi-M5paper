<!-- Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None -->

# KalliFunghi: NimBLE heap leak on init/deinit, full investigation on several MCUs (prompt for a separate chat)

Recommended: **Opus 5.5, effort high** (hard debugging inside a third-party BLE stack). Start in plan mode in `C:\dev\personal\KalliFunghi`. Start it after the branch `claude/ble-sync-ad8874` is merged into `main` (it contains the bench and the current mitigation).

---

## Prompt

Find the root cause of a heap leak in NimBLE's init/deinit cycle and decide on a fix. Read `firmware/README.md` (section "Bluetooth sync", bullet "Heap leak in NimBLE"), `firmware/bench/ble_leak/main.cpp`, `firmware/src/ble/BleSync.*`, the BLE part of `firmware/src/App.cpp` (`bleTick`, `plannedRestart`) and the env `ble_leak` in `firmware/platformio.ini`. Start in plan mode: propose the test matrix, the boards and the tools, and wait for my OK.

### What is known (2026-11-04)
- The KalliFunghi firmware (M5Paper, ESP32-D0WDQ6-V3 with PSRAM, Arduino core 3.3 on platform `espressif32 @ 8.0.0`, ESP-IDF 5.5 prebuilt libs, Bluedroid controller config `CONFIG_BTDM_CTRL_MODE_BTDM`) starts and stops NimBLE once per Bluetooth session (a tap on Sync opens 2 min; on USB it advertises all the time), to let the chip light-sleep in between. Earlier builds advertised 30 s every 5 min on battery, which is where the leak became visible (about 14 KB per day).
- Each `NimBLEDevice::init()` + `NimBLEDevice::deinit(true)` loses **48 B of internal heap**, always the same amount. Bench `ble_leak`, 20 cycles per stage:
  - A bare init/deinit: −48.0 B/cycle
  - B with server, service, 5 characteristics, advertising: −48.4 B/cycle
  - C the firmware's `kf::BleSync` start/stop: −48.0 B/cycle
  - D `esp_bt_controller_init/enable/disable/deinit` only, no NimBLE host: 0 B
- Identical in NimBLE-Arduino 2.3.9, 2.5.1 and 2.5.2. So it is in the NimBLE host, its FreeRTOS port (`nimble/porting/npl/freertos`, `esp_port/esp-hci`) or how NimBLE-Arduino calls `nimble_port_init/deinit`, not in KalliFunghi code.
- Reading the sources found matching delete calls for the obvious mutexes, semaphores and callouts (`preempt_done_mutex`, `adv_list_lock`, `ble_hs_mutex`, `ble_hs_hci_sem/mutex`, `ble_hs_stop_sem`, `vhci_send_sem`); not checked whether every deinit path actually runs. 48 B fits one small heap block, for example an `esp_timer` handle or a FreeRTOS object plus heap overhead.
- Current mitigation (keep it unless the fix makes it unnecessary): below 150 KB free the device restarts, on USB only; on battery the windows pause below 90 KB. A restart on battery is avoided because the GPIO2 power hold through a reset is unverified.

### Investigate
1. **Board matrix.** List the ESP32 boards I have (ask me) and propose at most one extra board to buy if a family is missing; use my vendor order from the global CLAUDE.md and prefer parts in stock. Families worth covering, because their BLE controllers and libraries differ: ESP32 classic (the M5Paper, plus a plain dev board without PSRAM if available), ESP32-S3, ESP32-C3, ESP32-C6 or H2. Optionally an nRF52 board with the n-able Arduino core, to see whether the leak follows the NimBLE host on a non-ESP platform.
2. **Matrix of software:** NimBLE-Arduino 1.4.x vs 2.x; Arduino core 3.3 vs the newest 3.x; with and without PSRAM; `deinit(true)` vs `deinit(false)`; and a plain ESP-IDF project using IDF's own NimBLE component (`nimble_port_init` / `nimble_port_deinit`, no NimBLE-Arduino). Reuse `bench/ble_leak` (copy it into a separate bench project per target if needed) and keep the output format, so all numbers compare.
3. **Find the allocation.** In the ESP-IDF project, turn on heap tracing (`CONFIG_HEAP_TRACING_STANDALONE`, leaks mode) around one init/deinit cycle and get the caller of the block that stays. Map it to the NimBLE source and to the Arduino wrapper.
4. **Fix or workaround.** Options to weigh: a patch in NimBLE-Arduino (local patch via `extra_scripts` or a fork pinned in `platformio.ini`), calling a missing deinit ourselves, keeping the host up and only stopping the controller/advertising if light sleep still works (check the controller's sleep clock: the prebuilt libs use the main crystal, `CONFIG_BTDM_CTRL_LPCLK_SEL_MAIN_XTAL`), or keeping the current restart mitigation. Measure what each costs in flash, RAM and current, and whether light sleep still works between windows.
5. **Upstream.** If the cause is in NimBLE-Arduino or esp-nimble, write the issue text (minimal repro, numbers per board and version, the allocating caller) into `docs/` for me to post. Do not post anything yourself.

### Rules
- One board per COM port; ask me before each flash and name the port. Run commands that take longer than 2 minutes in the background.
- No host gcc on this machine: bench results come from the boards. PlatformIO is at `~/.platformio/penv/Scripts/pio.exe`; KiCad's Python is the only Python.
- Work in your own branch; commit when I approve; don't push or merge into `main` without asking.
- Follow my global CLAUDE.md code-tagging rules. BLE, heap and power code are review priority High.
- Deliverables: a results table (board, core, NimBLE version, PSRAM, B/cycle) in `firmware/README.md` or a new `docs/nimble-leak.md`, the root cause with the allocating call, the chosen fix with its measured effect on the KalliFunghi firmware (at least 12 Bluetooth sessions with the heap after each, e.g. repeated Sync taps or a test build that starts sessions periodically), and a list of what you could not verify.
