#pragma once

#include "LXMFMessage.h"
#include "ReticulumManager.h"
#include "storage/MessageStore.h"
#include <Destination.h>
#include <Packet.h>
#include <Link.h>
#include <Identity.h>
#include <functional>
#include <deque>

class LXMFManager {
public:
    using MessageCallback = std::function<void(const LXMFMessage&)>;

    bool begin(ReticulumManager* rns, MessageStore* store);
    void loop();

    // Send a text message to a destination hash
    bool sendMessage(const RNS::Bytes& destHash, const std::string& content, const std::string& title = "");

    // Incoming message callback
    void setMessageCallback(MessageCallback cb) { _onMessage = cb; }

    // Queue info
    int queuedCount() const { return _outQueue.size(); }

    // Get all conversations (destination hashes with messages)
    const std::vector<std::string>& conversations() const;

    // Get messages for a conversation
    std::vector<LXMFMessage> getMessages(const std::string& peerHex) const;

    // Unread count for a peer (or total)
    int unreadCount(const std::string& peerHex = "") const;

    // Mark conversation as read
    void markRead(const std::string& peerHex);

private:
    bool sendDirect(LXMFMessage& msg);
    void processIncoming(const uint8_t* data, size_t len, const RNS::Bytes& destHash);

    // Static callbacks for microReticulum
    static void onPacketReceived(const RNS::Bytes& data, const RNS::Packet& packet);
    static void onLinkEstablished(RNS::Link& link);

    ReticulumManager* _rns = nullptr;
    MessageStore* _store = nullptr;
    MessageCallback _onMessage;
    std::deque<LXMFMessage> _outQueue;

    // Unread tracking (lazy-loaded on first access)
    void computeUnreadFromDisk();
    mutable bool _unreadComputed = false;
    mutable std::map<std::string, int> _unread;

    static LXMFManager* _instance;
};
