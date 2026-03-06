#pragma once

#include <map>
#include "ui/Screen.h"
#include "ui/widgets/ScrollList.h"
#include "reticulum/LXMFManager.h"

class MessagesScreen : public Screen {
public:
    void render(M5Canvas& canvas) override;
    bool handleKey(const KeyEvent& event) override;
    const char* title() const override { return "Messages"; }
    void onEnter() override;

    void setLXMFManager(LXMFManager* lxmf) { _lxmf = lxmf; }

    // Callback to open a conversation
    using OpenConversationCb = std::function<void(const std::string& peerHex)>;
    void setOpenCallback(OpenConversationCb cb) { _openCb = cb; }

private:
    void refreshList();

    LXMFManager* _lxmf = nullptr;
    ScrollList _list;
    std::vector<std::string> _peerHexes;  // Parallel to list items
    unsigned long _lastRefresh = 0;
    OpenConversationCb _openCb;
    std::map<std::string, std::string> _previewCache;
};
