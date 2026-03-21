#include "Keyboard.h"

void Keyboard::begin() {
    // M5Cardputer.begin() already initializes the keyboard
    _mode = InputMode::Navigation;
    _hasEvent = false;
}

void Keyboard::update() {
    _hasEvent = false;

    if (!M5Cardputer.Keyboard.isChange()) return;
    if (!M5Cardputer.Keyboard.isPressed()) return;

    auto& status = M5Cardputer.Keyboard.keysState();

    // Build event
    _event = {};
    _event.ctrl = status.ctrl;
    _event.shift = status.shift;
    _event.fn = status.fn;
    _event.alt = status.alt;
    _event.opt = status.opt;
    _event.enter = status.enter;
    _event.del = status.del;
    _event.tab = status.tab;
    _event.space = status.space;

    // Get first character if available
    if (!status.word.empty()) {
        _event.character = status.word[0];
    }

    // Fn + backtick = ESC on Cardputer Adv
    // The hardware ESC key shares the backtick key — Fn must be held
    if (_event.fn && _event.character == '`') {
        _event.character = 27;
    }

    // Handle double-tap Aa for caps lock
    if (_event.shift && !_event.ctrl && _event.character == 0 &&
        !_event.enter && !_event.del && !_event.tab && !_event.space) {
        unsigned long now = millis();
        if (now - _lastCapsToggle < 500) {
            M5Cardputer.Keyboard.setCapsLocked(!M5Cardputer.Keyboard.capslocked());
            _lastCapsToggle = 0;
        } else {
            _lastCapsToggle = now;
        }
    }

    _hasEvent = true;

    // Fire callback
    if (_keyCallback) {
        _keyCallback(_event);
    }
}

bool Keyboard::capsLocked() const {
    return M5Cardputer.Keyboard.capslocked();
}

void Keyboard::setCapsLocked(bool locked) {
    M5Cardputer.Keyboard.setCapsLocked(locked);
}
