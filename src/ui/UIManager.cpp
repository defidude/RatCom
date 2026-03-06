#include "UIManager.h"

void UIManager::begin() {
    _canvas.createSprite(Theme::SCREEN_W, Theme::SCREEN_H);
    _canvas.setColorDepth(16);
    _canvas.fillScreen(Theme::BG);
    _needsRender = true;
}

void UIManager::setScreen(Screen* screen) {
    if (_currentScreen) {
        _currentScreen->onExit();
    }
    _currentScreen = screen;
    if (_currentScreen) {
        _currentScreen->onEnter();
    }
    _needsRender = true;
}

void UIManager::render() {
    // Clear canvas
    _canvas.fillScreen(Theme::BG);

    if (_bootMode) {
        // Boot mode: full screen, no status/tab bars
        if (_currentScreen) {
            _currentScreen->render(_canvas);
        }
    } else {
        // Normal mode: status bar + clipped content + tab bar
        _statusBar.render(_canvas);

        if (_currentScreen) {
            _canvas.setClipRect(0, Theme::CONTENT_Y, Theme::CONTENT_W, Theme::CONTENT_H);
            _currentScreen->render(_canvas);
            _canvas.clearClipRect();
        }

        _tabBar.render(_canvas);

        // Render overlay on top of everything
        if (_overlay) {
            _canvas.setClipRect(0, Theme::CONTENT_Y, Theme::CONTENT_W, Theme::CONTENT_H);
            _overlay->render(_canvas);
            _canvas.clearClipRect();
        }
    }

    // Push to display
    flush();
}

void UIManager::flush() {
    _canvas.pushSprite(&M5.Display, 0, 0);
}

bool UIManager::handleKey(const KeyEvent& event) {
    if (_currentScreen) {
        return _currentScreen->handleKey(event);
    }
    return false;
}
