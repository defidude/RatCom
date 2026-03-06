#pragma once

#include <M5GFX.h>

class StatusBar {
public:
    void render(M5Canvas& canvas);

    void setTransportMode(const char* mode) { _transportMode = mode; }
    void setLoRaOnline(bool online) { _loraOnline = online; }
    void flashAnnounce() { _announceFlashUntil = millis() + 1500; }

private:
    const char* _transportMode = "STANDALONE";
    bool _loraOnline = false;
    unsigned long _announceFlashUntil = 0;
    float _smoothedBattery = -1.0f;
    unsigned long _lastBatteryRead = 0;
};
