#pragma once

#include <Arduino.h>
#include "storage/FlashStore.h"
#include "storage/SDStore.h"
#include "reticulum/LXMFMessage.h"
#include <vector>
#include <string>
#include <map>

class MessageStore {
public:
    bool begin(FlashStore* flash, SDStore* sd = nullptr);

    // Save a message (incoming or outgoing)
    bool saveMessage(const LXMFMessage& msg);

    // Load all messages for a conversation (by peer hex hash)
    std::vector<LXMFMessage> loadConversation(const std::string& peerHex) const;

    // Get list of conversations (peer hex hashes)
    const std::vector<std::string>& conversations() const { return _conversations; }

    // Refresh conversation list from filesystem
    void refreshConversations();

    // Get message count for a conversation
    int messageCount(const std::string& peerHex) const;

    // Delete a conversation
    bool deleteConversation(const std::string& peerHex);

    // Mark all incoming messages in a conversation as read
    void markConversationRead(const std::string& peerHex);

private:
    // Get directory path for a peer conversation (flash)
    String conversationDir(const std::string& peerHex) const;

    // Get directory path for a peer conversation (SD)
    String sdConversationDir(const std::string& peerHex) const;

    // Enforce max messages per conversation on a backend
    void enforceFlashLimit(const std::string& peerHex);
    void enforceSDLimit(const std::string& peerHex);

    // Migrate messages from flash to SD on first boot with SD
    void migrateFlashToSD();

    FlashStore* _flash = nullptr;
    SDStore* _sd = nullptr;
    std::vector<std::string> _conversations;
};
