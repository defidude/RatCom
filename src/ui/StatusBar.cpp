#include "StatusBar.h"
#include "Theme.h"
#include <M5Unified.h>

void StatusBar::render(M5Canvas& canvas) {
    // Background
    canvas.fillRect(0, 0, Theme::SCREEN_W, Theme::STATUS_BAR_H, Theme::BG);

    // Divider line
    canvas.drawFastHLine(0, Theme::STATUS_BAR_H - 1, Theme::SCREEN_W, Theme::BORDER);

    canvas.setTextSize(Theme::FONT_SIZE);

    // Battery indicator (left) — throttled + EMA smoothed
    unsigned long now = millis();
    if (_smoothedBattery < 0 || now - _lastBatteryRead >= 2000) {
        int raw = M5.Power.getBatteryLevel();
        if (_smoothedBattery < 0) {
            _smoothedBattery = (float)raw;
        } else {
            _smoothedBattery = _smoothedBattery * 0.85f + (float)raw * 0.15f;
        }
        _lastBatteryRead = now;
    }
    int batLevel = (int)(_smoothedBattery + 0.5f);
    if (batLevel < 0) batLevel = 0;
    if (batLevel > 100) batLevel = 100;
    char batStr[12];
    snprintf(batStr, sizeof(batStr), "%d%%", batLevel);
    uint16_t batColor = Theme::PRIMARY;  // green
    if (batLevel <= 10) batColor = Theme::ERROR;       // red
    else if (batLevel <= 30) batColor = Theme::WARNING; // yellow
    canvas.setTextColor(batColor);
    canvas.setCursor(Theme::STATUS_PAD, Theme::STATUS_PAD);
    canvas.print(batStr);

    // Transport mode (center) — flash "ANNOUNCED" briefly
    const char* centerText = _transportMode;
    uint16_t centerColor = Theme::PRIMARY;
    if (millis() < _announceFlashUntil) {
        centerText = "ANNOUNCED";
        centerColor = Theme::PRIMARY;
    }
    canvas.setTextColor(centerColor);
    int modeLen = strlen(centerText) * Theme::CHAR_W;
    canvas.setCursor((Theme::SCREEN_W - modeLen) / 2, Theme::STATUS_PAD);
    canvas.print(centerText);

    // LoRa indicator (right)
    canvas.setTextColor(_loraOnline ? Theme::PRIMARY : Theme::MUTED);
    const char* loraStr = _loraOnline ? "LoRa" : "----";
    int loraW = strlen(loraStr) * Theme::CHAR_W;
    canvas.setCursor(Theme::SCREEN_W - loraW - Theme::STATUS_PAD, Theme::STATUS_PAD);
    canvas.print(loraStr);
}
