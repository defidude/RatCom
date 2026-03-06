#pragma once

#include <M5GFX.h>
#include <vector>
#include <string>
#include "ui/Theme.h"

class ScrollList {
public:
    void setItems(const std::vector<std::string>& items);
    void addItem(const std::string& item);
    void clear();

    void render(M5Canvas& canvas, int x, int y, int w, int h);

    // Navigation
    void scrollUp();
    void scrollDown();
    void setSelected(int idx);
    int getSelectedIndex() const { return _selected; }
    const std::string& getSelectedItem() const;

    int itemCount() const { return _items.size(); }

private:
    std::vector<std::string> _items;
    int _selected = 0;
    int _scrollOffset = 0;
    int _visibleRows = 0;
};
