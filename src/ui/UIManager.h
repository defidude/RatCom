#pragma once

#include <M5Unified.h>
#include <M5GFX.h>
#include "Screen.h"
#include "StatusBar.h"
#include "TabBar.h"
#include "Theme.h"

class UIManager {
public:
    void begin();
    void render();

    // Screen management
    void setScreen(Screen* screen);
    Screen* getScreen() { return _currentScreen; }

    // Component access
    StatusBar& statusBar() { return _statusBar; }
    TabBar& tabBar() { return _tabBar; }
    M5Canvas& canvas() { return _canvas; }

    // Push canvas to display
    void flush();

    // Overlay (drawn on top of content after clip is cleared)
    void setOverlay(Screen* overlay) { _overlay = overlay; }

    // Boot mode — hides status bar and tab bar
    void setBootMode(bool boot) { _bootMode = boot; }
    bool isBootMode() const { return _bootMode; }

    // Handle key event — routes to current screen
    bool handleKey(const KeyEvent& event);

private:
    M5Canvas _canvas;
    StatusBar _statusBar;
    TabBar _tabBar;
    Screen* _currentScreen = nullptr;
    Screen* _overlay = nullptr;
    bool _needsRender = true;
    bool _bootMode = false;
};
