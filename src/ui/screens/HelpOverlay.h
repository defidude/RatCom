#pragma once

#include "ui/Screen.h"

class HelpOverlay : public Screen {
public:
    void render(M5Canvas& canvas) override;
    bool handleKey(const KeyEvent& event) override;
    const char* title() const override { return "Help"; }

    bool isVisible() const { return _visible; }
    void toggle() { _visible = !_visible; }
    void hide() { _visible = false; }

private:
    bool _visible = false;
};
