#include "ProgressBar.h"

void ProgressBar::setProgress(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    _progress = progress;
}

void ProgressBar::render(M5Canvas& canvas, int x, int y, int w, int h) {
    // Border
    canvas.drawRect(x, y, w, h, Theme::BORDER);

    // Fill
    int fillW = (int)((w - 2) * _progress);
    if (fillW > 0) {
        canvas.fillRect(x + 1, y + 1, fillW, h - 2, Theme::PRIMARY);
    }
}
