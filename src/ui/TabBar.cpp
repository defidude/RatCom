#include "TabBar.h"
#include "Theme.h"

constexpr const char* TabBar::TAB_LABELS[4];

void TabBar::setActiveTab(int tab) {
    if (tab >= 0 && tab < Theme::TAB_COUNT) {
        _activeTab = tab;
    }
}

void TabBar::cycleTab(int direction) {
    _activeTab = (_activeTab + direction + Theme::TAB_COUNT) % Theme::TAB_COUNT;
}

void TabBar::setUnreadCount(int tab, int count) {
    if (tab >= 0 && tab < Theme::TAB_COUNT) {
        _unreadCounts[tab] = count;
    }
}

void TabBar::render(M5Canvas& canvas) {
    int y = Theme::SCREEN_H - Theme::TAB_BAR_H;

    // Background
    canvas.fillRect(0, y, Theme::SCREEN_W, Theme::TAB_BAR_H, Theme::BG);

    // Top divider
    canvas.drawFastHLine(0, y, Theme::SCREEN_W, Theme::BORDER);

    canvas.setTextSize(Theme::FONT_SIZE);

    for (int i = 0; i < Theme::TAB_COUNT; i++) {
        int tx = i * Theme::TAB_W;
        bool active = (i == _activeTab);

        // Active tab underline
        if (active) {
            canvas.drawFastHLine(tx + 2, y + Theme::TAB_BAR_H - 2,
                                 Theme::TAB_W - 4, Theme::PRIMARY);
        }

        // Label — blink Msgs tab when unread
        uint16_t labelColor;
        if (active) {
            labelColor = Theme::TAB_ACTIVE;
        } else if (i == TAB_MSGS && _unreadCounts[TAB_MSGS] > 0) {
            bool blinkOn = ((millis() / 1500) % 2) == 0;
            labelColor = blinkOn ? Theme::WARNING : Theme::TAB_INACTIVE;
        } else {
            labelColor = Theme::TAB_INACTIVE;
        }
        canvas.setTextColor(labelColor);
        int labelLen = strlen(TAB_LABELS[i]) * Theme::CHAR_W;
        int labelX = tx + (Theme::TAB_W - labelLen) / 2;
        int labelY = y + (Theme::TAB_BAR_H - Theme::CHAR_H) / 2;
        canvas.setCursor(labelX, labelY);
        canvas.print(TAB_LABELS[i]);

        // Unread badge
        if (_unreadCounts[i] > 0) {
            char badge[8];
            snprintf(badge, sizeof(badge), "%d", _unreadCounts[i]);
            int badgeW = strlen(badge) * Theme::CHAR_W + 4;
            int badgeX = labelX + labelLen + 1;
            int badgeY = labelY - 1;
            canvas.fillRoundRect(badgeX, badgeY, badgeW, Theme::CHAR_H + 2, 2, Theme::BADGE_BG);
            canvas.setTextColor(Theme::BADGE_TEXT);
            canvas.setCursor(badgeX + 2, badgeY + 1);
            canvas.print(badge);
        }
    }
}
