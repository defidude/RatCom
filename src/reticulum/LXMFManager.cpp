#include "LXMFManager.h"
#include "config/Config.h"
#include <Transport.h>
#include <time.h>

LXMFManager* LXMFManager::_instance = nullptr;

bool LXMFManager::begin(ReticulumManager* rns, MessageStore* store) {
    _rns = rns;
    _store = store;
    _instance = this;

    // Register callbacks on the LXMF delivery destination
    RNS::Destination& dest = _rns->destination();
    dest.set_packet_callback(onPacketReceived);
    dest.set_link_established_callback(onLinkEstablished);

    Serial.println("[LXMF] Manager started");
    return true;
}

void LXMFManager::loop() {
    if (_outQueue.empty()) return;
    LXMFMessage& msg = _outQueue.front();

    // Throttle retries — wait 2 seconds between attempts
    unsigned long now = millis();
    if (msg.retries > 0 && (now - _lastRetryMs) < 2000) return;
    _lastRetryMs = now;

    if (sendDirect(msg)) {
        Serial.printf("[LXMF] Queue drain: status=%s dest=%s\n",
                      msg.statusStr(), msg.destHash.toHex().substr(0, 8).c_str());
        // Fire status callback for UI update (no second disk save — status is ephemeral)
        if (_statusCb) {
            std::string peerHex = msg.destHash.toHex();
            _statusCb(peerHex, msg.timestamp, msg.status);
        }
        _outQueue.pop_front();
    }
}

bool LXMFManager::sendMessage(const RNS::Bytes& destHash, const std::string& content, const std::string& title) {
    LXMFMessage msg;
    msg.sourceHash = _rns->destination().hash();
    msg.destHash = destHash;
    time_t now = time(nullptr);
    if (now > 1700000000) {
        msg.timestamp = (double)now;
    } else {
        msg.timestamp = millis() / 1000.0;
    }
    msg.content = content;
    msg.title = title;
    msg.incoming = false;
    msg.status = LXMFStatus::QUEUED;

    if ((int)_outQueue.size() >= RATPUTER_MAX_OUTQUEUE) {
        Serial.printf("[LXMF] WARNING: Outgoing queue full (%d), dropping oldest\n",
                      (int)_outQueue.size());
        _outQueue.pop_front();
    }

    _outQueue.push_back(msg);

    Serial.printf("[LXMF] Message queued for %s (%d bytes)\n",
                  destHash.toHex().substr(0, 8).c_str(), (int)content.size());
    return true;
}

bool LXMFManager::sendDirect(LXMFMessage& msg) {
    RNS::Identity recipientId = RNS::Identity::recall(msg.destHash);
    if (!recipientId) {
        msg.retries++;
        // Proactively request path on first retry and every 10 retries
        if (msg.retries == 1 || msg.retries % 10 == 0) {
            RNS::Transport::request_path(msg.destHash);
            Serial.printf("[LXMF] Requested path for %s\n",
                          msg.destHash.toHex().substr(0, 8).c_str());
        }
        if (msg.retries >= 30) {
            Serial.printf("[LXMF] recall FAILED for %s after %d retries\n",
                          msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
            msg.status = LXMFStatus::FAILED;
            return true;
        }
        Serial.printf("[LXMF] recall pending for %s (retry %d/30)\n",
                      msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
        return false;
    }

    Serial.printf("[LXMF] recall OK: identity for %s\n",
                  msg.destHash.toHex().substr(0, 8).c_str());

    RNS::Destination outDest(
        recipientId,
        RNS::Type::Destination::OUT,
        RNS::Type::Destination::SINGLE,
        "lxmf",
        "delivery"
    );

    // Verify destination hash matches expected
    std::string outDestHex = outDest.hash().toHex();
    std::string expectedHex = msg.destHash.toHex();
    if (outDestHex != expectedHex) {
        Serial.printf("[LXMF] ERROR: dest hash mismatch! computed=%s expected=%s\n",
                      outDestHex.substr(0, 16).c_str(), expectedHex.substr(0, 16).c_str());
    } else {
        Serial.printf("[LXMF] outDest hash verified: %s\n", outDestHex.substr(0, 8).c_str());
    }

    std::vector<uint8_t> payload = msg.packFull(_rns->identity());
    if (payload.empty()) {
        Serial.println("[LXMF] Failed to pack message");
        msg.status = LXMFStatus::FAILED;
        return true;
    }
    RNS::Bytes payloadBytes(payload.data(), payload.size());

    if (payloadBytes.size() > RNS::Type::Reticulum::MDU) {
        Serial.printf("[LXMF] Message too large for direct delivery: %d bytes\n",
                      (int)payloadBytes.size());
        msg.status = LXMFStatus::FAILED;
        return true;
    }

    msg.status = LXMFStatus::SENDING;
    RNS::Packet packet(outDest, payloadBytes);
    RNS::PacketReceipt receipt = packet.send();

    if (receipt) {
        msg.status = LXMFStatus::SENT;
        msg.messageId = RNS::Identity::full_hash(payloadBytes);
        Serial.printf("[LXMF] Sent %d bytes to %s\n",
                      (int)payloadBytes.size(),
                      msg.destHash.toHex().substr(0, 8).c_str());
    } else {
        msg.status = LXMFStatus::FAILED;
        Serial.printf("[LXMF] Send FAILED to %s\n",
                      msg.destHash.toHex().substr(0, 8).c_str());
    }

    return true;
}

void LXMFManager::onPacketReceived(const RNS::Bytes& data, const RNS::Packet& packet) {
    if (!_instance) return;
    Serial.printf("[LXMF] Packet received: %d bytes\n", (int)data.size());
    _instance->processIncoming(data.data(), data.size(), packet.destination_hash());
}

void LXMFManager::onLinkEstablished(RNS::Link& link) {
    if (!_instance) return;
    Serial.println("[LXMF] Link established");

    link.set_packet_callback([](const RNS::Bytes& data, const RNS::Packet& packet) {
        if (!_instance) return;
        Serial.printf("[LXMF] Link message received: %d bytes\n", (int)data.size());
        _instance->processIncoming(data.data(), data.size(), packet.destination_hash());
    });
}

void LXMFManager::processIncoming(const uint8_t* data, size_t len, const RNS::Bytes& destHash) {
    LXMFMessage msg;
    if (!LXMFMessage::unpackFull(data, len, msg)) {
        Serial.println("[LXMF] Failed to unpack message");
        return;
    }

    // Drop self-messages (loopback from Transport)
    if (_rns && msg.sourceHash == _rns->destination().hash()) {
        Serial.println("[LXMF] Dropping loopback self-message");
        return;
    }

    // Deduplication: skip messages we've already processed
    std::string msgIdHex = msg.messageId.toHex();
    if (_seenMessageIds.count(msgIdHex)) {
        Serial.printf("[LXMF] Duplicate message from %s (already seen)\n",
                      msg.sourceHash.toHex().substr(0, 8).c_str());
        return;
    }
    _seenMessageIds.insert(msgIdHex);
    if ((int)_seenMessageIds.size() > MAX_SEEN_IDS) {
        _seenMessageIds.erase(_seenMessageIds.begin());
    }

    msg.destHash = destHash;

    Serial.printf("[LXMF] From %s: \"%s\"\n",
                  msg.sourceHash.toHex().substr(0, 8).c_str(),
                  msg.content.c_str());

    // Log back-pressure status
    if (_store && _store->writeQueue().isFull()) {
        Serial.println("[LXMF] WARNING: Write queue full (back-pressure)");
    }

    // Store message (async via WriteQueue)
    if (_store) {
        _store->saveMessage(msg);
    }

    // Track unread
    std::string peerHex = msg.sourceHash.toHex();
    _unread[peerHex]++;

    // Notify callback
    if (_onMessage) {
        _onMessage(msg);
    }
}

const std::vector<std::string>& LXMFManager::conversations() const {
    if (_store) return _store->conversations();
    static std::vector<std::string> empty;
    return empty;
}

std::vector<LXMFMessage> LXMFManager::getMessages(const std::string& peerHex, int limit) const {
    if (_store) return _store->loadConversation(peerHex, limit);
    return {};
}

int LXMFManager::unreadCount(const std::string& peerHex) const {
    if (!_unreadComputed) {
        const_cast<LXMFManager*>(this)->computeUnreadFromDisk();
    }
    if (peerHex.empty()) {
        int total = 0;
        for (auto& kv : _unread) total += kv.second;
        return total;
    }
    auto it = _unread.find(peerHex);
    return (it != _unread.end()) ? it->second : 0;
}

void LXMFManager::computeUnreadFromDisk() {
    _unreadComputed = true;
    if (!_store) return;

    // Use efficient file-based unread counting (no JSON parsing needed)
    for (auto& conv : _store->conversations()) {
        int count = _store->unreadCountForPeer(conv);
        if (count > 0) _unread[conv] = count;
    }

    int totalUnread = 0;
    for (auto& kv : _unread) totalUnread += kv.second;
    if (totalUnread > 0) {
        Serial.printf("[LXMF] Restored %d unread messages\n", totalUnread);
    }
}

void LXMFManager::markRead(const std::string& peerHex) {
    _unread[peerHex] = 0;
    if (_store) {
        _store->markConversationRead(peerHex);
    }
}
