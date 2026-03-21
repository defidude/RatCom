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
    unsigned long now = millis();

    for (auto it = _outQueue.begin(); it != _outQueue.end(); ++it) {
        LXMFMessage& msg = *it;

        // Per-message retry cooldown: 2 seconds between attempts
        if (msg.retries > 0 && (now - msg.lastRetryMs) < 2000) continue;

        msg.lastRetryMs = now;

        if (sendDirect(msg)) {
            Serial.printf("[LXMF] Queue drain: status=%s dest=%s\n",
                          msg.statusStr(), msg.destHash.toHex().substr(0, 8).c_str());
            if (_statusCb) {
                std::string peerHex = msg.destHash.toHex();
                _statusCb(peerHex, msg.timestamp, msg.status);
            }
            _outQueue.erase(it);
            return;  // One send per loop() call to avoid hogging CPU
        }
        // sendDirect returned false — message stays in queue, try next
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

    // Proactively request path so it's ready when sendDirect runs
    if (!RNS::Transport::has_path(destHash)) {
        RNS::Transport::request_path(destHash);
        Serial.printf("[LXMF] Message queued for %s (%d bytes) — requesting path\n",
                      destHash.toHex().substr(0, 8).c_str(), (int)content.size());
    } else {
        Serial.printf("[LXMF] Message queued for %s (%d bytes) — path known\n",
                      destHash.toHex().substr(0, 8).c_str(), (int)content.size());
    }
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

    // Ensure path exists — without a path, Transport::outbound() broadcasts as
    // Header1 which the Python hub silently drops
    if (!RNS::Transport::has_path(msg.destHash)) {
        msg.retries++;
        if (msg.retries == 1 || msg.retries % 5 == 0) {
            Serial.printf("[LXMF] No path for %s, requesting (retry %d)\n",
                          msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
            RNS::Transport::request_path(msg.destHash);
        }
        if (msg.retries >= 30) {
            Serial.printf("[LXMF] No path for %s after %d retries — FAILED\n",
                          msg.destHash.toHex().substr(0, 8).c_str(), msg.retries);
            msg.status = LXMFStatus::FAILED;
            return true;
        }
        return false;  // keep in queue, retry later
    }

    Serial.printf("[LXMF] path OK: %s hops=%d\n",
                  msg.destHash.toHex().substr(0, 8).c_str(),
                  RNS::Transport::hops_to(msg.destHash));

    RNS::Destination outDest(
        recipientId,
        RNS::Type::Destination::OUT,
        RNS::Type::Destination::SINGLE,
        "lxmf",
        "delivery"
    );

    // packFull returns opportunistic format: [src:16][sig:64][msgpack]
    std::vector<uint8_t> payload = msg.packFull(_rns->identity());
    if (payload.empty()) {
        Serial.println("[LXMF] Failed to pack message");
        msg.status = LXMFStatus::FAILED;
        return true;
    }

    msg.status = LXMFStatus::SENDING;
    bool sent = false;

    // Try link-based delivery if we have an active link to this peer
    if (_outLink && _outLinkDestHash == msg.destHash
        && _outLink.status() == RNS::Type::Link::ACTIVE) {
        // Link delivery: prepend dest_hash (Python DIRECT format)
        std::vector<uint8_t> linkPayload;
        linkPayload.reserve(16 + payload.size());
        linkPayload.insert(linkPayload.end(), msg.destHash.data(), msg.destHash.data() + 16);
        linkPayload.insert(linkPayload.end(), payload.begin(), payload.end());
        RNS::Bytes linkBytes(linkPayload.data(), linkPayload.size());
        if (linkBytes.size() <= RNS::Type::Reticulum::MDU) {
            Serial.printf("[LXMF] sending via link: %d bytes to %s\n",
                          (int)linkBytes.size(), msg.destHash.toHex().substr(0, 8).c_str());
            RNS::Packet packet(_outLink, linkBytes);
            RNS::PacketReceipt receipt = packet.send();
            if (receipt) { sent = true; }
        }
    }

    // Fallback: opportunistic delivery (always available, no delay)
    if (!sent) {
        RNS::Bytes payloadBytes(payload.data(), payload.size());
        if (payloadBytes.size() > RNS::Type::Reticulum::MDU) {
            Serial.printf("[LXMF] payload too large: %d > MDU\n", (int)payloadBytes.size());
            msg.status = LXMFStatus::FAILED;
            return true;
        }
        Serial.printf("[LXMF] sending opportunistic: %d bytes to %s\n",
                      (int)payloadBytes.size(), msg.destHash.toHex().substr(0, 8).c_str());
        RNS::Packet packet(outDest, payloadBytes);
        RNS::PacketReceipt receipt = packet.send();
        if (receipt) { sent = true; }
    }

    if (sent) {
        msg.status = LXMFStatus::SENT;
        // messageId already computed by packFull() matching Python's LXMessage.pack()
        Serial.printf("[LXMF] SENT OK: msgId=%s\n", msg.messageId.toHex().substr(0, 8).c_str());
    } else {
        msg.status = LXMFStatus::FAILED;
        Serial.printf("[LXMF] Send FAILED to %s\n",
                      msg.destHash.toHex().substr(0, 8).c_str());
    }

    // Background: establish link for future messages to this peer
    if (!_outLinkPending && (!_outLink || _outLinkDestHash != msg.destHash
        || _outLink.status() == RNS::Type::Link::CLOSED)) {
        _outLinkPendingHash = msg.destHash;
        _outLinkPending = true;
        Serial.printf("[LXMF] Establishing link to %s for future messages\n",
                      msg.destHash.toHex().substr(0, 8).c_str());
        RNS::Link newLink(outDest, onOutLinkEstablished, onOutLinkClosed);
    }

    return true;
}

void LXMFManager::onPacketReceived(const RNS::Bytes& data, const RNS::Packet& packet) {
    if (!_instance) return;
    // Non-link delivery: dest_hash is NOT in LXMF payload (it's in the RNS packet header).
    // Reconstruct full format by prepending it, matching Python LXMRouter.delivery_packet().
    const RNS::Bytes& destHash = packet.destination_hash();
    std::vector<uint8_t> fullData;
    fullData.reserve(destHash.size() + data.size());
    fullData.insert(fullData.end(), destHash.data(), destHash.data() + destHash.size());
    fullData.insert(fullData.end(), data.data(), data.data() + data.size());
    _instance->processIncoming(fullData.data(), fullData.size(), destHash);
}

void LXMFManager::onOutLinkEstablished(RNS::Link& link) {
    if (!_instance) return;
    _instance->_outLink = link;
    _instance->_outLinkDestHash = _instance->_outLinkPendingHash;
    _instance->_outLinkPending = false;
    Serial.printf("[LXMF] Outbound link established to %s\n",
                  _instance->_outLinkDestHash.toHex().substr(0, 8).c_str());
}

void LXMFManager::onOutLinkClosed(RNS::Link& link) {
    if (!_instance) return;
    _instance->_outLink = {RNS::Type::NONE};
    _instance->_outLinkPending = false;
    Serial.println("[LXMF] Outbound link closed");
}

void LXMFManager::onLinkEstablished(RNS::Link& link) {
    if (!_instance) return;
    Serial.printf("[LXMF-DIAG] onLinkEstablished fired! link_id=%s status=%d\n",
        link.link_id().toHex().substr(0, 16).c_str(), (int)link.status());
    link.set_packet_callback([](const RNS::Bytes& data, const RNS::Packet& packet) {
        if (!_instance) return;
        Serial.printf("[LXMF-DIAG] Link packet received! %d bytes pkt_dest=%s\n",
            (int)data.size(), packet.destination_hash().toHex().substr(0, 16).c_str());
        // Link delivery: data already contains [dest:16][src:16][sig:64][msgpack]
        // Do NOT use packet.destination_hash() — that's the link_id, not the LXMF dest.
        _instance->processIncoming(data.data(), data.size(), RNS::Bytes());
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

    // Only overwrite destHash if caller provided a real one (non-link delivery).
    // For link delivery, unpackFull already parsed the correct destHash from the payload.
    if (destHash.size() > 0) {
        msg.destHash = destHash;
    }

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
