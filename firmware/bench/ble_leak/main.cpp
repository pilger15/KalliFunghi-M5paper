/* Created by Claude Opus 5.5 | Owner: Julian Martus | Review priority: High | Reviewed by: None */
// Bench: free heap across repeated NimBLE start/stop cycles, to find where the firmware's
// ~48 B per advertising window go. Three stages of 20 cycles each, printed over UART0
// (USB, 115200):
//   A  NimBLEDevice::init + deinit(true)
//   B  A plus server, service, five characteristics and advertising start
//   C  kf::BleSync::start / stop (the firmware's session object)
//   D  esp_bt_controller init / enable / disable / deinit only (no NimBLE host)
// A stage that loses heap every cycle points at the layer it adds.

#include <Arduino.h>
#include <M5Unified.h>
#include <NimBLEDevice.h>
#include <esp_bt.h>
#include <esp_heap_caps.h>
#include "ble/BleSync.h"
#include "config.h"
#include "hw/Board.h"

namespace {

constexpr int kCycles = 20;

/* Created by Claude Opus 5.5 | Review priority: Low | Reviewed by: None */
uint32_t freeInternal() { return heap_caps_get_free_size(MALLOC_CAP_INTERNAL); }

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void stageA()
{
    NimBLEDevice::init(kf::kBleName);
    delay(200);
    NimBLEDevice::deinit(true);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void stageB()
{
    NimBLEDevice::init(kf::kBleName);
    NimBLEServer* s = NimBLEDevice::createServer();
    NimBLEService* svc = s->createService(kf::kBleServiceUuid);
    svc->createCharacteristic(kf::kBleStatusUuid, NIMBLE_PROPERTY::READ);
    svc->createCharacteristic(kf::kBleTimeUuid, NIMBLE_PROPERTY::WRITE);
    svc->createCharacteristic(kf::kBleLiveUuid, NIMBLE_PROPERTY::NOTIFY);
    svc->createCharacteristic(kf::kBleFilesUuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY);
    svc->createCharacteristic(kf::kBleControlUuid, NIMBLE_PROPERTY::WRITE_NR);
    s->start();
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    NimBLEAdvertisementData ad;
    ad.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
    ad.addServiceUUID(NimBLEUUID(kf::kBleServiceUuid));
    adv->setAdvertisementData(ad);
    adv->start();
    delay(200);
    NimBLEDevice::deinit(true);
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
// Only the ESP-IDF controller, no NimBLE host.
void stageD()
{
    esp_bt_controller_config_t cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&cfg) != ESP_OK) { Serial.println("D: controller init failed"); return; }
    esp_bt_controller_enable(ESP_BT_MODE_BLE);
    delay(200);
    esp_bt_controller_disable();
    esp_bt_controller_deinit();
}

kf::BleSync g_sync;

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void stageC()
{
    g_sync.start(millis(), 0);
    delay(200);
    g_sync.stop();
}

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void run(const char* name, void (*fn)())
{
    uint32_t first = 0;
    uint32_t prev = 0;
    for (int i = 0; i < kCycles; ++i) {
        fn();
        delay(100);
        uint32_t h = freeInternal();
        if (i == 0) { first = h; }
        Serial.printf("%s %2d free %u (%+d)\n", name, i, (unsigned)h, i ? (int)(h - prev) : 0);
        prev = h;
    }
    Serial.printf("%s: %+d B over %d cycles after the first, %.1f B/cycle\n", name, (int)(prev - first),
                  kCycles - 1, (double)((int)(prev - first)) / (kCycles - 1));
}

} // namespace

/* Created by Claude Opus 5.5 | Review priority: Medium | Reviewed by: None */
void setup()
{
    kf::board::earlyInit();
    Serial.begin(115200);
    auto cfg = M5.config();
    cfg.serial_baudrate = 0;
    cfg.clear_display = false;
    M5.begin(cfg);
    delay(1000);
    Serial.printf("ble_leak bench, NimBLE-Arduino, free internal %u\n", (unsigned)freeInternal());
    run("A", stageA);
    run("B", stageB);
    run("C", stageC);
    run("D", stageD);
    Serial.println("done");
}

void loop() { delay(1000); }
