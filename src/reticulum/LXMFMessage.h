#pragma once

#include <Arduino.h>
#include <Bytes.h>
#include <Identity.h>
#include <string>
#include <vector>

// Delivery status (local tracking)
enum class LXMFStatus : uint8_t {
    DRAFT = 0,
    QUEUED,
    SENDING,
    SENT,
    DELIVERED,
    FAILED
};

struct LXMFMessage {
    RNS::Bytes sourceHash;     // 16 bytes — sender destination hash
    RNS::Bytes destHash;       // 16 bytes — recipient destination hash
    double timestamp = 0;
    std::string content;
    std::string title;
    RNS::Bytes signature;      // 64 bytes Ed25519

    // Local metadata (not part of wire format)
    LXMFStatus status = LXMFStatus::DRAFT;
    bool incoming = false;
    bool read = false;          // Persistence: false = unread
    int retries = 0;
    RNS::Bytes messageId;      // SHA-256 hash of full payload
    uint32_t receiveCounter = 0; // Monotonic receive order counter

    // === Wire format (opportunistic) ===
    // dest_hash(16) + source_hash(16) + signature(64) + msgpack([timestamp, title, content, fields_map])
    // Signature covers: dest_hash + source_hash + msgpack_content + message_hash

    // Pack content portion only (for signing)
    static std::vector<uint8_t> packContent(double timestamp, const std::string& content, const std::string& title);

    // Pack full wire payload (source_hash + content + signature)
    std::vector<uint8_t> packFull(const RNS::Identity& signingIdentity) const;

    // Unpack from wire payload
    static bool unpackFull(const uint8_t* data, size_t len, LXMFMessage& msg);

    // Status display string
    const char* statusStr() const;
};
