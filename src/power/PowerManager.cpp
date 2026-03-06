#include "PowerManager.h"

void PowerManager::begin() {
    _lastActivity = millis();
    _state = ACTIVE;
    M5.Display.setBrightness(_fullBrightness);
}

void PowerManager::activity() {
    _lastActivity = millis();
    if (_state != ACTIVE) {
        setState(ACTIVE);
    }
}

void PowerManager::loop() {
    unsigned long elapsed = millis() - _lastActivity;

    switch (_state) {
        case ACTIVE:
            if (_offTimeout > 0 && elapsed >= _offTimeout) {
                setState(SCREEN_OFF);
            } else if (_dimTimeout > 0 && elapsed >= _dimTimeout) {
                setState(DIMMED);
            }
            break;

        case DIMMED:
            if (_offTimeout > 0 && elapsed >= _offTimeout) {
                setState(SCREEN_OFF);
            }
            break;

        case SCREEN_OFF:
            // Stay off until activity()
            break;
    }
}

void PowerManager::setState(State newState) {
    if (newState == _state) return;
    _state = newState;

    switch (_state) {
        case ACTIVE:
            M5.Display.wakeup();
            M5.Display.setBrightness(_fullBrightness);
            break;
        case DIMMED:
            M5.Display.setBrightness(DIM_BRIGHTNESS);
            break;
        case SCREEN_OFF:
            M5.Display.setBrightness(0);
            M5.Display.sleep();
            break;
    }
}
