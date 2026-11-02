/* Edited by Claude Opus 5.5 | Owner: Julian Martus | Review priority: Medium | Reviewed by: None */
// Power trace for current measurements (build env m5paper_debug).
// Output goes to Serial2 TX on Grove port C (GPIO18, 115200 baud) through an
// external 3.3 V USB-UART adapter (GND + RX only), so the board can run from the
// battery or a bench supply without USB. UART0 is not used: GPIO1/GPIO3 belong
// to USB detection.
// Env m5paper_usbtrace (KF_TRACE_USB) sends the same trace over UART0 to the USB
// port instead, for bench debugging on USB; GPIO1 then stays the UART TX.
#pragma once

#if defined(KF_TRACE_USB)
#include <Arduino.h>
#define KF_TRACE_ENABLED 1
#define KF_TRACE_BEGIN() Serial.begin(115200)
#define KF_TRACE(...) do { Serial.printf(__VA_ARGS__); Serial.print("\n"); Serial.flush(); } while (0)
#elif defined(KF_DEBUG_POWER)
#include <Arduino.h>
#define KF_TRACE_ENABLED 1
#define KF_TRACE_BEGIN() Serial2.begin(115200, SERIAL_8N1, -1, 18)
#define KF_TRACE(...) do { Serial2.printf(__VA_ARGS__); Serial2.print("\n"); Serial2.flush(); } while (0)
#else
#define KF_TRACE_ENABLED 0
#define KF_TRACE_BEGIN() do {} while (0)
#define KF_TRACE(...) do {} while (0)
#endif
