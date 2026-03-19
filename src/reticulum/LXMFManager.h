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
#include <set>

class LXMFManager {
public:
    using MessageCallback = std::function<void(const LXMFMessage&)>;
    using StatusCallback = std::function<void(const std::string& peerHex, double timestamp, LXMFStatus status)>;

    bool begin(ReticulumManager* rns, MessageStore* store);
    void loop();

    // Send a text message to a destination hash
    bool sendMessage(const RNS::Bytes& destHash, const std::string& content, const std::string& title = "");

    // Incoming message callback
    void setMessageCallback(MessageCallback cb) { _onMessage = cb; }

    // Status callback (fires when send completes with SENT/FAILED)
    void setStatusCallback(StatusCallback cb) { _statusCb = cb; }

    // Queue info
    int queuedCount() const { return _outQueue.size(); }

    // Get all conversations (destination hashes with messages)
    const std::vector<std::string>& conversations() const;

    // Get messages for a conversation (paginated, last N messages)
    std::vector<LXMFMessage> getMessages(const std::string& peerHex, int limit = 20) const;

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
    static void onOutLinkEstablished(RNS::Link& link);
    static void onOutLinkClosed(RNS::Link& link);

    ReticulumManager* _rns = nullptr;
    MessageStore* _store = nullptr;
    MessageCallback _onMessage;
    StatusCallback _statusCb;
    std::deque<LXMFMessage> _outQueue;

    // Outbound link state (opportunistic-first, link upgrades in background)
    RNS::Link _outLink{RNS::Type::NONE};
    RNS::Bytes _outLinkDestHash;
    bool _outLinkPending = false;

    // Unread tracking (lazy-loaded on first access)
    void computeUnreadFromDisk();
    mutable bool _unreadComputed = false;
    mutable std::map<std::string, int> _unread;

    // Deduplication: recently seen message IDs
    std::set<std::string> _seenMessageIds;
    static constexpr int MAX_SEEN_IDS = 100;

    static LXMFManager* _instance;
};
