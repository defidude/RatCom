#pragma once

#include <M5GFX.h>

class TabBar {
public:
    void render(M5Canvas& canvas);

    int getActiveTab() const { return _activeTab; }
    void setActiveTab(int tab);
    void cycleTab(int direction);

    void setUnreadCount(int tab, int count);

    static constexpr int TAB_HOME  = 0;
    static constexpr int TAB_MSGS  = 1;
    static constexpr int TAB_NODES = 2;
    static constexpr int TAB_SETUP = 3;

private:
    int _activeTab = 0;
    int _unreadCounts[4] = {0};
    static constexpr const char* TAB_LABELS[4] = {"Home", "Msgs", "Nodes", "Setup"};
};
