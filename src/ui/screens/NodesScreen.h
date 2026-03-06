#pragma once

#include <vector>
#include <string>
#include <functional>
#include "ui/Screen.h"
#include "ui/widgets/ScrollList.h"
#include "reticulum/AnnounceManager.h"

class NodesScreen : public Screen {
public:
    void setAnnounceManager(AnnounceManager* mgr) { _announces = mgr; }

    // Callback when user selects a node (for starting conversation)
    using NodeSelectedCb = std::function<void(const std::string& peerHex)>;
    void setNodeSelectedCallback(NodeSelectedCb cb) { _selectCb = cb; }

    // Callback for save/unsave contact
    using NodeSaveCb = std::function<void(const std::string& peerHex, bool save)>;
    void setNodeSaveCallback(NodeSaveCb cb) { _saveCb = cb; }

    void onEnter() override;
    void onExit() override;
    void render(M5Canvas& canvas) override;
    bool handleKey(const KeyEvent& event) override;
    const char* title() const override { return "Nodes"; }

private:
    void refreshList();
    void showActionMenu(int nodeIdx);
    void executeAction(int actionIdx);
    void exitActionMenu();

    AnnounceManager* _announces = nullptr;
    ScrollList _list;
    std::vector<std::string> _nodeHashes;
    unsigned long _lastRefresh = 0;
    NodeSelectedCb _selectCb;
    NodeSaveCb _saveCb;

    // Action menu state
    bool _showingActions = false;
    ScrollList _actionList;
    int _selectedNodeIdx = -1;
    std::string _selectedNodeName;
    std::string _selectedNodeHash;
    bool _selectedNodeSaved = false;

    // Contacts view toggle
    bool _contactsView = false;
};
