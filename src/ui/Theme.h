#pragma once

#include <M5GFX.h>

// =============================================================================
// Ratputer — Cyberpunk Theme Constants
// =============================================================================

namespace Theme {

// --- Colors (RGB565) ---
constexpr uint16_t BG           = 0x0000;  // #000000 pure black
constexpr uint16_t PRIMARY      = 0x07E8;  // #00FF41 signal green (approx)
constexpr uint16_t SECONDARY    = 0x0664;  // #00CC33 dimmed green
constexpr uint16_t MUTED        = 0x3326;  // #336633 dark green
constexpr uint16_t ERROR        = 0xF986;  // #FF3333 red
constexpr uint16_t WARNING      = 0xFFE0;  // #FFFF00 bright yellow
constexpr uint16_t BORDER       = 0x0220;  // #004400 subtle dark green
constexpr uint16_t SELECTION_BG = 0x0180;  // #003300 dark green
constexpr uint16_t TAB_ACTIVE   = PRIMARY;
constexpr uint16_t TAB_INACTIVE = MUTED;
constexpr uint16_t BADGE_BG     = ERROR;
constexpr uint16_t BADGE_TEXT   = BG;

// --- Layout Metrics ---
constexpr int SCREEN_W       = 240;
constexpr int SCREEN_H       = 135;
constexpr int STATUS_BAR_H   = 12;
constexpr int TAB_BAR_H      = 18;
constexpr int CONTENT_Y      = STATUS_BAR_H;
constexpr int CONTENT_H      = SCREEN_H - STATUS_BAR_H - TAB_BAR_H;
constexpr int CONTENT_W      = SCREEN_W;

// --- Font ---
constexpr const lgfx::GFXfont* FONT_SMALL = nullptr;  // Built-in 6x8
constexpr int FONT_SIZE       = 1;
constexpr int CHAR_W          = 6;
constexpr int CHAR_H          = 8;

// --- Tab Bar ---
constexpr int TAB_COUNT       = 4;
constexpr int TAB_W           = SCREEN_W / TAB_COUNT;

// --- Status Bar ---
constexpr int STATUS_PAD      = 2;

}  // namespace Theme
