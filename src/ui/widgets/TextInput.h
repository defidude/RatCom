#pragma once

#include <M5GFX.h>
#include <string>
#include "input/Keyboard.h"
#include "ui/Theme.h"

class TextInput {
public:
    void render(M5Canvas& canvas, int x, int y, int w);

    // Handle key event. Returns true if consumed.
    bool handleKey(const KeyEvent& event);

    // Content access
    const std::string& getText() const { return _text; }
    void setText(const std::string& text);
    void clear();

    // State
    bool isActive() const { return _active; }
    void setActive(bool active) { _active = active; }
    void setMaxLength(int len) { _maxLength = len; }
    void setNumericOnly(bool numeric) { _numericOnly = numeric; }

    // Callback when Enter is pressed
    using SubmitCallback = std::function<void(const std::string&)>;
    void setSubmitCallback(SubmitCallback cb) { _submitCb = cb; }

private:
    std::string _text;
    int _cursorPos = 0;
    bool _active = false;
    bool _cursorVisible = true;
    unsigned long _lastBlink = 0;
    int _maxLength = 200;
    bool _numericOnly = false;
    SubmitCallback _submitCb;
};
