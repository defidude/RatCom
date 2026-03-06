#pragma once

#include <M5GFX.h>
#include "ui/Theme.h"

class ProgressBar {
public:
    void render(M5Canvas& canvas, int x, int y, int w, int h);

    void setProgress(float progress);  // 0.0 to 1.0
    float getProgress() const { return _progress; }

private:
    float _progress = 0.0f;
};
