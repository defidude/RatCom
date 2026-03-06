#pragma once

#include <Arduino.h>

class BLEStub {
public:
    bool begin();
    void loop();
    void stop();

    bool isAdvertising() const { return _advertising; }

private:
    bool _advertising = false;
};
