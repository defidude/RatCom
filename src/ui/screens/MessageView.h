#pragma once

#include "ui/Screen.h"
#include "ui/widgets/TextInput.h"
#include "reticulum/LXMFManager.h"
#include <vector>
#include <string>

class MessageView : public Screen {
public:
    void render(M5Canvas& canvas) override;
    bool handleKey(const KeyEvent& event) override;
    const char* title() const override { return "Chat"; }
    void onEnter() override;

    void setLXMFManager(LXMFManager* lxmf) { _lxmf = lxmf; }
    void setPeerHex(const std::string& peerHex) { _peerHex = peerHex; }

    // Callback to return to messages list
    using BackCallback = std::function<void()>;
    void setBackCallback(BackCallback cb) { _backCb = cb; }

private:
    void refreshMessages();
    void sendCurrentInput();

    // Cached display lines
    struct ChatLine {
        std::string text;
        uint16_t color;
    };

    LXMFManager* _lxmf = nullptr;
    std::string _peerHex;
    std::vector<LXMFMessage> _messages;
    std::vector<ChatLine> _chatLines;
    int _scrollOffset = 0;
    TextInput _input;
    unsigned long _lastRefresh = 0;
    BackCallback _backCb;
};
