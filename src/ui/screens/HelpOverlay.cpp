#include "HelpOverlay.h"
#include "ui/Theme.h"

void HelpOverlay::render(M5Canvas& canvas) {
    if (!_visible) return;

    // Semi-transparent overlay (draw dark background)
    int oy = Theme::CONTENT_Y;
    canvas.fillRect(10, oy + 2, Theme::CONTENT_W - 20, Theme::CONTENT_H - 4, Theme::BG);
    canvas.drawRect(10, oy + 2, Theme::CONTENT_W - 20, Theme::CONTENT_H - 4, Theme::BORDER);

    int x = 16;
    int y = oy + 6;

    canvas.setTextColor(Theme::PRIMARY);
    canvas.setTextSize(Theme::FONT_SIZE);
    canvas.drawString("HOTKEYS", x, y); y += 10;

    canvas.drawFastHLine(x, y, Theme::CONTENT_W - 40, Theme::BORDER); y += 3;

    canvas.setTextColor(Theme::SECONDARY);
    canvas.drawString("Ctrl+H  Help (this)", x, y); y += 9;
    canvas.drawString("Ctrl+M  Messages", x, y); y += 9;
    canvas.drawString("Ctrl+N  New message", x, y); y += 9;
    canvas.drawString("Ctrl+S  Settings", x, y); y += 9;
    canvas.drawString("Ctrl+A  Announce", x, y); y += 9;
    canvas.drawString("Ctrl+D  Diagnostics", x, y); y += 9;
    canvas.drawString(",//     Cycle tabs", x, y); y += 9;
    canvas.drawString(";/.     Navigate", x, y); y += 9;
    canvas.drawString("Enter   Select", x, y); y += 9;
    canvas.drawString("Esc     Back", x, y); y += 11;

    canvas.setTextColor(Theme::MUTED);
    canvas.drawString("Press Ctrl+H to close", x, y);
}

bool HelpOverlay::handleKey(const KeyEvent& event) {
    if (_visible) {
        // Any key closes the overlay
        _visible = false;
        return true;
    }
    return false;
}
