#include "BootScreen.h"
#include "ui/Theme.h"
#include "config/Config.h"

void BootScreen::setProgress(float progress, const char* status) {
    _progress = progress;
    _progressBar.setProgress(progress);
    if (status) _statusText = status;
}

void BootScreen::render(M5Canvas& canvas) {
    canvas.fillScreen(Theme::BG);

    // "RATSPEAK" in large text (size 2 = 12x16 chars)
    canvas.setTextSize(2);
    canvas.setTextColor(Theme::PRIMARY);
    const char* title = "RATSPEAK";
    int titleW = strlen(title) * 12;  // size 2 = 12px wide
    canvas.setCursor((Theme::SCREEN_W - titleW) / 2, 20);
    canvas.print(title);

    // ".ORG" in normal text below
    canvas.setTextSize(Theme::FONT_SIZE);
    canvas.setTextColor(Theme::SECONDARY);
    const char* subtitle = ".ORG";
    int subW = strlen(subtitle) * Theme::CHAR_W;
    canvas.setCursor((Theme::SCREEN_W - subW) / 2, 40);
    canvas.print(subtitle);

    // Version
    char verStr[32];
    snprintf(verStr, sizeof(verStr), "v%s", RATPUTER_VERSION_STRING);
    canvas.setTextColor(Theme::MUTED);
    int verW = strlen(verStr) * Theme::CHAR_W;
    canvas.setCursor((Theme::SCREEN_W - verW) / 2, 56);
    canvas.print(verStr);

    // Progress bar (near bottom)
    int barY = Theme::SCREEN_H - 38;
    _progressBar.render(canvas, 30, barY, Theme::SCREEN_W - 60, 8);

    // Status text below progress bar
    canvas.setTextColor(Theme::SECONDARY);
    int statusLen = strlen(_statusText) * Theme::CHAR_W;
    canvas.setCursor((Theme::SCREEN_W - statusLen) / 2, barY + 12);
    canvas.print(_statusText);
}
