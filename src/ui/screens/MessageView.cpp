#include "MessageView.h"
#include "ui/Theme.h"
#include <time.h>

void MessageView::onEnter() {
    _input.setActive(true);
    _input.clear();
    _input.setMaxLength(400);
    _input.setSubmitCallback([this](const std::string& text) {
        sendCurrentInput();
    });
    refreshMessages();
}

void MessageView::refreshMessages() {
    if (!_lxmf || _peerHex.empty()) return;

    _messages = _lxmf->getMessages(_peerHex);

    // Dedup: same content + timestamp → keep highest-status version
    for (size_t i = 0; i < _messages.size(); i++) {
        for (size_t j = i + 1; j < _messages.size(); ) {
            if (_messages[i].timestamp == _messages[j].timestamp &&
                _messages[i].content == _messages[j].content) {
                if ((int)_messages[j].status > (int)_messages[i].status)
                    _messages[i] = _messages[j];
                _messages.erase(_messages.begin() + j);
            } else {
                j++;
            }
        }
    }

    _lxmf->markRead(_peerHex);

    // Build chat lines (IRC-style)
    _chatLines.clear();
    for (const auto& msg : _messages) {
        ChatLine line;

        // Smart timestamp: real HH:MM if epoch, relative if uptime
        char ts[16];
        double tsVal = msg.timestamp;
        if (tsVal > 1700000000) {
            time_t t = (time_t)tsVal;
            struct tm* tm = localtime(&t);
            snprintf(ts, sizeof(ts), "%02d:%02d", tm->tm_hour, tm->tm_min);
        } else {
            unsigned long ago = (millis() / 1000) - (unsigned long)tsVal;
            if (ago < 60) snprintf(ts, sizeof(ts), "%lus", ago);
            else if (ago < 3600) snprintf(ts, sizeof(ts), "%lum", ago / 60);
            else snprintf(ts, sizeof(ts), "%luh", ago / 3600);
        }

        if (msg.incoming) {
            // Sender name (short hash)
            std::string sender = msg.sourceHash.toHex().substr(0, 4);
            line.text = std::string(ts) + " " + sender + "> " + msg.content;
            line.color = Theme::PRIMARY;
        } else {
            line.text = std::string(ts) + " you> " + msg.content;

            // Color indicates status — no text suffix needed
            switch (msg.status) {
                case LXMFStatus::SENT:
                case LXMFStatus::DELIVERED:
                    line.color = Theme::PRIMARY;    // Green = confirmed
                    break;
                case LXMFStatus::FAILED:
                    line.color = Theme::ERROR;      // Red = failed
                    break;
                default:  // QUEUED, SENDING, DRAFT
                    line.color = Theme::WARNING;    // Yellow = pending
                    break;
            }
        }

        _chatLines.push_back(line);
    }

    // Auto-scroll to bottom
    int maxCharsPerLine = Theme::CONTENT_W / Theme::CHAR_W;
    int totalWrappedLines = 0;
    for (const auto& cl : _chatLines) {
        totalWrappedLines += ((int)cl.text.size() / maxCharsPerLine) + 1;
    }
    int visibleLines = (Theme::CONTENT_H - 24) / Theme::CHAR_H;  // Minus header + input
    _scrollOffset = std::max(0, totalWrappedLines - visibleLines);

    _lastRefresh = millis();
}

void MessageView::render(M5Canvas& canvas) {
    // Auto-refresh every 3 seconds
    if (millis() - _lastRefresh > 10000) {
        refreshMessages();
    }

    int baseY = Theme::CONTENT_Y;

    // Header: peer hash + back hint
    std::string header;
    if (_peerHex.size() >= 8) {
        header = _peerHex.substr(0, 4) + ":" + _peerHex.substr(4, 4);
    } else {
        header = _peerHex;
    }
    canvas.setTextColor(Theme::PRIMARY);
    canvas.setTextSize(Theme::FONT_SIZE);
    canvas.drawString(header.c_str(), 4, baseY);

    canvas.setTextColor(Theme::MUTED);
    canvas.drawString("[Esc:back]", Theme::CONTENT_W - 60, baseY);

    // Separator
    canvas.drawFastHLine(0, baseY + 9, Theme::CONTENT_W, Theme::BORDER);

    // Chat area
    int chatY = baseY + 11;
    int chatH = Theme::CONTENT_H - 24;  // Leave room for input
    int maxCharsPerLine = Theme::CONTENT_W / Theme::CHAR_W;
    int lineY = chatY;
    int currentLine = 0;

    for (const auto& cl : _chatLines) {
        // Word-wrap the line
        int lineLen = cl.text.size();
        int pos = 0;
        while (pos < lineLen) {
            int remaining = lineLen - pos;
            int chars = std::min(remaining, maxCharsPerLine);

            if (currentLine >= _scrollOffset) {
                int drawY = chatY + (currentLine - _scrollOffset) * Theme::CHAR_H;
                if (drawY + Theme::CHAR_H > chatY + chatH) break;  // Past visible area

                canvas.setTextColor(cl.color);
                std::string segment = cl.text.substr(pos, chars);
                canvas.drawString(segment.c_str(), 2, drawY);
            }

            pos += chars;
            currentLine++;
        }
    }

    // Input separator
    int inputY = baseY + Theme::CONTENT_H - 12;
    canvas.drawFastHLine(0, inputY - 2, Theme::CONTENT_W, Theme::BORDER);

    // Text input
    _input.render(canvas, 0, inputY, Theme::CONTENT_W);
}

bool MessageView::handleKey(const KeyEvent& event) {
    // Escape = back to messages list
    if (event.character == 27) {  // ESC
        if (_backCb) _backCb();
        return true;
    }

    // Pass to text input
    if (_input.handleKey(event)) {
        return true;
    }

    return false;
}

void MessageView::sendCurrentInput() {
    if (!_lxmf || _peerHex.empty()) return;

    std::string text = _input.getText();
    if (text.empty()) return;

    // Convert peer hex to Bytes
    RNS::Bytes destHash;
    destHash.assignHex(_peerHex.c_str());

    _lxmf->sendMessage(destHash, text);
    _input.clear();

    // Refresh to show sent message
    refreshMessages();
}
