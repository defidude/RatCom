#pragma once

#include <M5GFX.h>
#include "input/Keyboard.h"

class Screen {
public:
    virtual ~Screen() = default;

    // Called when this screen becomes active
    virtual void onEnter() {}

    // Called when this screen is deactivated
    virtual void onExit() {}

    // Render content area to the canvas
    virtual void render(M5Canvas& canvas) = 0;

    // Handle key event. Return true if consumed.
    virtual bool handleKey(const KeyEvent& event) { return false; }

    // Screen title for tab/title bar
    virtual const char* title() const = 0;
};
