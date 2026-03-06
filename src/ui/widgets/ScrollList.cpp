#include "ScrollList.h"

static const std::string EMPTY_ITEM;

void ScrollList::setItems(const std::vector<std::string>& items) {
    _items = items;
    _selected = 0;
    _scrollOffset = 0;
}

void ScrollList::addItem(const std::string& item) {
    _items.push_back(item);
}

void ScrollList::clear() {
    _items.clear();
    _selected = 0;
    _scrollOffset = 0;
}

void ScrollList::setSelected(int idx) {
    if (idx >= 0 && idx < (int)_items.size()) {
        _selected = idx;
        if (_visibleRows > 0) {
            if (_selected < _scrollOffset) _scrollOffset = _selected;
            else if (_selected >= _scrollOffset + _visibleRows)
                _scrollOffset = _selected - _visibleRows + 1;
        }
    }
}

void ScrollList::scrollUp() {
    if (_items.empty()) return;
    if (_selected > 0) {
        _selected--;
    } else {
        _selected = (int)_items.size() - 1;  // Wrap to bottom
    }
    if (_selected < _scrollOffset) {
        _scrollOffset = _selected;
    }
    if (_selected >= _scrollOffset + _visibleRows) {
        _scrollOffset = _selected - _visibleRows + 1;
    }
}

void ScrollList::scrollDown() {
    if (_items.empty()) return;
    if (_selected < (int)_items.size() - 1) {
        _selected++;
    } else {
        _selected = 0;  // Wrap to top
        _scrollOffset = 0;
    }
    if (_selected >= _scrollOffset + _visibleRows) {
        _scrollOffset = _selected - _visibleRows + 1;
    }
}

const std::string& ScrollList::getSelectedItem() const {
    if (_selected >= 0 && _selected < (int)_items.size()) {
        return _items[_selected];
    }
    return EMPTY_ITEM;
}

void ScrollList::render(M5Canvas& canvas, int x, int y, int w, int h) {
    int rowH = Theme::CHAR_H + 4;
    _visibleRows = h / rowH;

    canvas.setTextSize(Theme::FONT_SIZE);

    for (int i = 0; i < _visibleRows && (i + _scrollOffset) < (int)_items.size(); i++) {
        int idx = i + _scrollOffset;
        int ry = y + i * rowH;

        // Selection highlight
        if (idx == _selected) {
            canvas.fillRect(x, ry, w, rowH, Theme::SELECTION_BG);
            canvas.setTextColor(Theme::PRIMARY);
        } else {
            canvas.setTextColor(Theme::SECONDARY);
        }

        // Truncate text to fit width
        int maxChars = w / Theme::CHAR_W;
        canvas.setCursor(x + 2, ry + 2);
        if ((int)_items[idx].length() > maxChars) {
            canvas.print(_items[idx].substr(0, maxChars - 2).c_str());
            canvas.print("..");
        } else {
            canvas.print(_items[idx].c_str());
        }
    }

    // Scroll indicator
    if ((int)_items.size() > _visibleRows) {
        int barH = h * _visibleRows / _items.size();
        if (barH < 4) barH = 4;
        int barY = y + (_scrollOffset * h / _items.size());
        canvas.fillRect(x + w - 2, barY, 2, barH, Theme::MUTED);
    }
}
