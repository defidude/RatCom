#include "NodesScreen.h"
#include "ui/Theme.h"
#include <algorithm>

void NodesScreen::onEnter() {
    refreshList();
}

void NodesScreen::onExit() {
    _showingActions = false;
}

void NodesScreen::refreshList() {
    if (!_announces) return;

    // Remember currently selected node hash
    std::string prevSelected;
    int oldIdx = _list.getSelectedIndex();
    if (oldIdx >= 0 && oldIdx < (int)_nodeHashes.size()) {
        prevSelected = _nodeHashes[oldIdx];
    }

    _list.clear();
    _nodeHashes.clear();

    // Copy and sort nodes alphabetically by name (case-insensitive)
    auto nodes = _announces->nodes();  // copy
    std::sort(nodes.begin(), nodes.end(), [](const DiscoveredNode& a, const DiscoveredNode& b) {
        std::string na = a.name, nb = b.name;
        std::transform(na.begin(), na.end(), na.begin(), ::tolower);
        std::transform(nb.begin(), nb.end(), nb.begin(), ::tolower);
        return na < nb;
    });

    int newSelectedIdx = 0;
    for (size_t i = 0; i < nodes.size(); i++) {
        const auto& node = nodes[i];

        // In contacts view, only show saved nodes
        if (_contactsView && !node.saved) continue;

        char line[64];
        std::string displayName = node.name.substr(0, 18);
        // Prefix saved nodes with * in all-nodes view
        if (!_contactsView && node.saved) {
            displayName = "*" + displayName;
        }
        unsigned long ago = (millis() - node.lastSeen) / 1000;
        if (ago < 60) {
            snprintf(line, sizeof(line), "%-20s %3lus %dhop",
                     displayName.c_str(), ago, node.hops);
        } else {
            snprintf(line, sizeof(line), "%-20s %3lum %dhop",
                     displayName.c_str(), ago / 60, node.hops);
        }
        _list.addItem(line);

        std::string hexStr = node.hash.toHex();
        _nodeHashes.push_back(hexStr);

        if (!prevSelected.empty() && hexStr == prevSelected) {
            newSelectedIdx = (int)_nodeHashes.size() - 1;
        }
    }

    if (!_nodeHashes.empty()) {
        _list.setSelected(newSelectedIdx);
    }

    _lastRefresh = millis();
}

void NodesScreen::showActionMenu(int nodeIdx) {
    if (!_announces || nodeIdx < 0 || nodeIdx >= (int)_nodeHashes.size()) return;

    _selectedNodeIdx = nodeIdx;
    _selectedNodeHash = _nodeHashes[nodeIdx];

    // Find node details
    RNS::Bytes hash;
    hash.assignHex(_selectedNodeHash.c_str());
    const DiscoveredNode* node = _announces->findNode(hash);
    if (!node) return;

    _selectedNodeName = node->name;
    _selectedNodeSaved = node->saved;

    _actionList.clear();
    _actionList.addItem("Message");
    if (_selectedNodeSaved) {
        _actionList.addItem("Remove Contact");
    } else {
        _actionList.addItem("Save Contact");
    }
    _actionList.addItem("Back");
    _actionList.setSelected(0);

    _showingActions = true;
}

void NodesScreen::executeAction(int actionIdx) {
    const std::string& action = _actionList.getSelectedItem();

    if (action == "Message") {
        exitActionMenu();
        if (_selectCb) {
            Serial.printf("[NODES] Selected: %s\n", _selectedNodeHash.c_str());
            _selectCb(_selectedNodeHash);
        }
    } else if (action == "Save Contact") {
        if (_saveCb) _saveCb(_selectedNodeHash, true);
        exitActionMenu();
        refreshList();
    } else if (action == "Remove Contact") {
        if (_saveCb) _saveCb(_selectedNodeHash, false);
        exitActionMenu();
        refreshList();
    } else {
        exitActionMenu();
    }
}

void NodesScreen::exitActionMenu() {
    _showingActions = false;
    _selectedNodeIdx = -1;
}

void NodesScreen::render(M5Canvas& canvas) {
    // Auto-refresh every 20 seconds (skip while action menu open)
    if (!_showingActions && millis() - _lastRefresh > 20000) {
        refreshList();
    }

    int y = Theme::CONTENT_Y;

    if (_showingActions) {
        // Action menu header
        canvas.setTextSize(Theme::FONT_SIZE);
        canvas.setTextColor(Theme::PRIMARY);
        canvas.setCursor(4, y + 2);
        canvas.print(_selectedNodeName.c_str());
        y += Theme::CHAR_H + 4;
        canvas.drawFastHLine(0, y, Theme::SCREEN_W, Theme::BORDER);
        y += 2;

        _actionList.render(canvas, 0, y, Theme::SCREEN_W, Theme::CONTENT_H - (y - Theme::CONTENT_Y));
    } else {
        // Header
        canvas.setTextSize(Theme::FONT_SIZE);
        canvas.setTextColor(Theme::PRIMARY);
        canvas.setCursor(4, y + 2);
        if (_contactsView) {
            canvas.printf("Contacts (%d)", (int)_nodeHashes.size());
        } else {
            canvas.printf("Nodes (%d)", _announces ? _announces->nodeCount() : 0);
        }

        // Bottom hint for toggle
        canvas.setTextColor(Theme::MUTED);
        const char* hint = _contactsView ? "[c] All Nodes" : "[c] Contacts";
        int hintW = strlen(hint) * Theme::CHAR_W;
        canvas.setCursor(Theme::SCREEN_W - hintW - 4, y + 2);
        canvas.print(hint);

        y += Theme::CHAR_H + 4;
        canvas.drawFastHLine(0, y, Theme::SCREEN_W, Theme::BORDER);
        y += 2;

        if (_list.itemCount() == 0) {
            canvas.setTextColor(Theme::MUTED);
            canvas.setCursor(4, y + 10);
            if (_contactsView) {
                canvas.print("No saved contacts.");
                canvas.setCursor(4, y + 22);
                canvas.print("Select a node and Save.");
            } else {
                canvas.print("No nodes discovered yet.");
                canvas.setCursor(4, y + 22);
                canvas.print("Waiting for announces...");
            }
        } else {
            _list.render(canvas, 0, y, Theme::SCREEN_W, Theme::CONTENT_H - (y - Theme::CONTENT_Y));
        }
    }
}

bool NodesScreen::handleKey(const KeyEvent& event) {
    if (_showingActions) {
        // ESC exits action menu
        if (event.character == 27) {
            exitActionMenu();
            return true;
        }
        if (event.character == ';') {
            _actionList.scrollUp();
            return true;
        }
        if (event.character == '.') {
            _actionList.scrollDown();
            return true;
        }
        if (event.enter) {
            executeAction(_actionList.getSelectedIndex());
            return true;
        }
        return true;  // Consume all keys while action menu is open
    }

    // Normal list navigation
    // ; = up arrow, . = down arrow on Cardputer Adv keyboard
    if (event.character == ';') {
        _list.scrollUp();
        return true;
    }
    if (event.character == '.') {
        _list.scrollDown();
        return true;
    }
    if (event.enter) {
        int idx = _list.getSelectedIndex();
        if (idx >= 0 && idx < (int)_nodeHashes.size()) {
            showActionMenu(idx);
        }
        return true;
    }
    // 'c' toggles contacts view
    if (event.character == 'c' || event.character == 'C') {
        _contactsView = !_contactsView;
        refreshList();
        return true;
    }
    return false;
}
