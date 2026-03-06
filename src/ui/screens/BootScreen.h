#pragma once

#include "ui/Screen.h"
#include "ui/widgets/ProgressBar.h"

class BootScreen : public Screen {
public:
    void render(M5Canvas& canvas) override;
    bool handleKey(const KeyEvent& event) override { return false; }
    const char* title() const override { return "Boot"; }

    void setProgress(float progress, const char* status = nullptr);
    bool isComplete() const { return _progress >= 1.0f; }

private:
    ProgressBar _progressBar;
    float _progress = 0.0f;
    const char* _statusText = "Initializing...";
};
