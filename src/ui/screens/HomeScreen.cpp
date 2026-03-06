#include "HomeScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"

static const char* detectPresetName(const UserSettings& s) {
    if (s.loraSF == 9 && s.loraBW == 250000 && s.loraCR == 5 && s.loraTxPower == 14)
        return "Balanced";
    if (s.loraSF == 12 && s.loraBW == 125000 && s.loraCR == 8 && s.loraTxPower == 17)
        return "Long Range";
    if (s.loraSF == 7 && s.loraBW == 500000 && s.loraCR == 5 && s.loraTxPower == 10)
        return "Fast";
    return "Custom";
}

bool HomeScreen::handleKey(const KeyEvent& event) {
    if (event.enter) {
        if (_announceCb) _announceCb();
        _announceFlashUntil = millis() + 1500;
        return true;
    }
    return false;
}

void HomeScreen::render(M5Canvas& canvas) {
    int y = Theme::CONTENT_Y + 4;
    int lineH = Theme::CHAR_H + 3;
    canvas.setTextSize(Theme::FONT_SIZE);

    // Identity hash
    canvas.setTextColor(Theme::PRIMARY);
    canvas.setCursor(4, y);
    canvas.print("ID: ");
    if (_rns) {
        canvas.print(_rns->identityHash());
    } else {
        canvas.print("not initialized");
    }
    y += lineH;

    // Transport status
    canvas.setTextColor(Theme::SECONDARY);
    canvas.setCursor(4, y);
    canvas.print("Transport: ");
    if (_rns && _rns->isTransportActive()) {
        canvas.setTextColor(Theme::PRIMARY);
        canvas.print("ACTIVE");
    } else {
        canvas.setTextColor(Theme::MUTED);
        canvas.print("OFFLINE");
    }
    y += lineH;

    // Path count
    canvas.setTextColor(Theme::SECONDARY);
    canvas.setCursor(4, y);
    canvas.printf("Paths: %d  Links: %d",
        _rns ? (int)_rns->pathCount() : 0,
        _rns ? (int)_rns->linkCount() : 0);
    y += lineH;

    // Divider
    canvas.drawFastHLine(4, y, Theme::SCREEN_W - 8, Theme::BORDER);
    y += 4;

    // Radio params with preset name
    canvas.setTextColor(Theme::SECONDARY);
    canvas.setCursor(4, y);
    if (_radio && _radio->isRadioOnline()) {
        if (_userConfig) {
            const char* preset = detectPresetName(_userConfig->settings());
            canvas.printf("LoRa: %s [SF%d %luk]",
                preset,
                _radio->getSpreadingFactor(),
                (unsigned long)(_radio->getSignalBandwidth() / 1000));
        } else {
            canvas.printf("LoRa: SF%d BW%luk",
                _radio->getSpreadingFactor(),
                (unsigned long)(_radio->getSignalBandwidth() / 1000));
        }
    } else {
        canvas.setTextColor(Theme::MUTED);
        canvas.print("LoRa: OFFLINE");
    }
    y += lineH;

    // TX power + frequency
    if (_radio && _radio->isRadioOnline()) {
        canvas.setTextColor(Theme::SECONDARY);
        canvas.setCursor(4, y);
        canvas.printf("%.1f MHz  TX: %d dBm  CR: %d",
            _radio->getFrequency() / 1000000.0,
            _radio->getTxPower(), _radio->getCodingRate4());
    }
    y += lineH;

    // Divider
    canvas.drawFastHLine(4, y, Theme::SCREEN_W - 8, Theme::BORDER);
    y += 4;

    // Announce line
    canvas.setTextColor(Theme::SECONDARY);
    canvas.setCursor(4, y);
    if (_rns && _rns->lastAnnounceTime() > 0) {
        unsigned long ago = (millis() - _rns->lastAnnounceTime()) / 1000;
        if (ago < 60) {
            canvas.printf("Announce: %lus ago", ago);
        } else {
            canvas.printf("Announce: %lum ago", ago / 60);
        }
    } else {
        canvas.print("Announce: never");
    }
    canvas.setTextColor(Theme::MUTED);
    canvas.print("  [Enter]");
    y += lineH;

    // Uptime & memory
    canvas.setTextColor(Theme::MUTED);
    canvas.setCursor(4, y);
    unsigned long upSec = millis() / 1000;
    unsigned long upMin = upSec / 60;
    unsigned long upHr = upMin / 60;
    canvas.printf("Up: %lu:%02lu:%02lu  Heap: %luK",
        upHr, upMin % 60, upSec % 60,
        (unsigned long)(ESP.getFreeHeap() / 1024));

    // Announce flash toast
    if (millis() < _announceFlashUntil) {
        const char* msg = "Announced!";
        int tw = strlen(msg) * Theme::CHAR_W + 12;
        int th = Theme::CHAR_H + 8;
        int tx = (Theme::CONTENT_W - tw) / 2;
        int ty = Theme::CONTENT_Y + Theme::CONTENT_H - th - 4;
        canvas.fillRoundRect(tx, ty, tw, th, 3, Theme::SELECTION_BG);
        canvas.drawRoundRect(tx, ty, tw, th, 3, Theme::PRIMARY);
        canvas.setTextColor(Theme::PRIMARY);
        canvas.setCursor(tx + 6, ty + 4);
        canvas.print(msg);
    }
}
