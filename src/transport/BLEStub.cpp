#include "BLEStub.h"
#include "config/Config.h"

#if HAS_BLE
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Ratputer BLE service UUID (custom)
#define RATPUTER_SERVICE_UUID    "a7f3c8e2-0001-4b6d-9f12-4a7e3d5c8b01"
#define RATPUTER_RX_CHAR_UUID    "a7f3c8e2-0002-4b6d-9f12-4a7e3d5c8b01"
#define RATPUTER_TX_CHAR_UUID    "a7f3c8e2-0003-4b6d-9f12-4a7e3d5c8b01"

static BLEServer* bleServer = nullptr;
static BLECharacteristic* txChar = nullptr;

class StubServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* server) override {
        Serial.println("[BLE] Client connected (stub)");
    }
    void onDisconnect(BLEServer* server) override {
        Serial.println("[BLE] Client disconnected");
        // Restart advertising
        server->getAdvertising()->start();
    }
};

bool BLEStub::begin() {
    BLEDevice::init("Ratputer");
    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(new StubServerCallbacks());

    // Create GATT service
    BLEService* service = bleServer->createService(RATPUTER_SERVICE_UUID);

    // RX characteristic (write from client)
    BLECharacteristic* rxChar = service->createCharacteristic(
        RATPUTER_RX_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE
    );
    rxChar->setValue("Ratputer v" RATPUTER_VERSION_STRING " — BLE stub");

    // TX characteristic (notify to client)
    txChar = service->createCharacteristic(
        RATPUTER_TX_CHAR_UUID,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    txChar->addDescriptor(new BLE2902());

    service->start();

    // Start advertising
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(RATPUTER_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->start();

    _advertising = true;
    Serial.println("[BLE] Advertising started (stub)");
    return true;
}

void BLEStub::loop() {
    // No-op for stub — v1.1 will implement Sideband protocol
}

void BLEStub::stop() {
    if (_advertising) {
        BLEDevice::getAdvertising()->stop();
        _advertising = false;
    }
}

#else
// BLE disabled
bool BLEStub::begin() { return false; }
void BLEStub::loop() {}
void BLEStub::stop() {}
#endif
