#pragma once

#include "ui/Screen.h"
#include "reticulum/ReticulumManager.h"
#include "radio/SX1262.h"
#include "config/UserConfig.h"

class HomeScreen : public Screen {
public:
    void setReticulumManager(ReticulumManager* mgr) { _rns = mgr; }
    void setRadio(SX1262* radio) { _radio = radio; }
    void setUserConfig(UserConfig* cfg) { _userConfig = cfg; }

    void render(M5Canvas& canvas) override;
    bool handleKey(const KeyEvent& event) override;
    const char* title() const override { return "Home"; }

private:
    ReticulumManager* _rns = nullptr;
    SX1262* _radio = nullptr;
    UserConfig* _userConfig = nullptr;
    unsigned long _announceFlashUntil = 0;
};
