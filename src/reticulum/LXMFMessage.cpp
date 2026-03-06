#include "LXMFMessage.h"
#include <cstring>

// =============================================================================
// MsgPack helpers — minimal encoder/decoder for LXMF content
// =============================================================================

static void mpPackFloat64(std::vector<uint8_t>& buf, double val) {
    buf.push_back(0xCB);  // float64
    uint64_t bits;
    memcpy(&bits, &val, 8);
    for (int i = 7; i >= 0; i--) {
        buf.push_back((bits >> (i * 8)) & 0xFF);
    }
}

static void mpPackString(std::vector<uint8_t>& buf, const std::string& str) {
    size_t len = str.size();
    if (len < 32) {
        buf.push_back(0xA0 | (uint8_t)len);  // fixstr
    } else if (len < 256) {
        buf.push_back(0xD9);  // str8
        buf.push_back((uint8_t)len);
    } else {
        buf.push_back(0xDA);  // str16
        buf.push_back((len >> 8) & 0xFF);
        buf.push_back(len & 0xFF);
    }
    buf.insert(buf.end(), str.begin(), str.end());
}

// Pack as MsgPack bin type (0xC4/0xC5) — LXMF expects title/content as raw bytes
static void mpPackBin(std::vector<uint8_t>& buf, const std::string& str) {
    size_t len = str.size();
    if (len < 256) {
        buf.push_back(0xC4);  // bin8
        buf.push_back((uint8_t)len);
    } else {
        buf.push_back(0xC5);  // bin16
        buf.push_back((len >> 8) & 0xFF);
        buf.push_back(len & 0xFF);
    }
    buf.insert(buf.end(), str.begin(), str.end());
}

// Read MsgPack float64
static bool mpReadFloat64(const uint8_t* data, size_t len, size_t& pos, double& val) {
    if (pos >= len || data[pos] != 0xCB) return false;
    pos++;
    if (pos + 8 > len) return false;
    uint64_t bits = 0;
    for (int i = 0; i < 8; i++) {
        bits = (bits << 8) | data[pos++];
    }
    memcpy(&val, &bits, 8);
    return true;
}

// Read MsgPack string or bin (LXMF uses bin for title/content)
static bool mpReadString(const uint8_t* data, size_t len, size_t& pos, std::string& str) {
    if (pos >= len) return false;
    uint8_t b = data[pos];
    size_t slen = 0;

    if ((b & 0xE0) == 0xA0) {
        // fixstr
        slen = b & 0x1F;
        pos++;
    } else if (b == 0xD9) {
        // str8
        pos++;
        if (pos >= len) return false;
        slen = data[pos++];
    } else if (b == 0xDA) {
        // str16
        pos++;
        if (pos + 2 > len) return false;
        slen = ((size_t)data[pos] << 8) | data[pos + 1];
        pos += 2;
    } else if (b == 0xC4) {
        // bin8
        pos++;
        if (pos >= len) return false;
        slen = data[pos++];
    } else if (b == 0xC5) {
        // bin16
        pos++;
        if (pos + 2 > len) return false;
        slen = ((size_t)data[pos] << 8) | data[pos + 1];
        pos += 2;
    } else {
        return false;
    }

    if (pos + slen > len) return false;
    str.assign((const char*)&data[pos], slen);
    pos += slen;
    return true;
}

// Skip MsgPack value (for skipping fields map)
static bool mpSkipValue(const uint8_t* data, size_t len, size_t& pos);

static bool mpSkipValue(const uint8_t* data, size_t len, size_t& pos) {
    if (pos >= len) return false;
    uint8_t b = data[pos];

    // fixstr
    if ((b & 0xE0) == 0xA0) {
        size_t slen = b & 0x1F;
        pos += 1 + slen;
        return pos <= len;
    }
    // fixmap
    if ((b & 0xF0) == 0x80) {
        size_t count = b & 0x0F;
        pos++;
        for (size_t i = 0; i < count * 2; i++) {
            if (!mpSkipValue(data, len, pos)) return false;
        }
        return true;
    }
    // fixarray
    if ((b & 0xF0) == 0x90) {
        size_t count = b & 0x0F;
        pos++;
        for (size_t i = 0; i < count; i++) {
            if (!mpSkipValue(data, len, pos)) return false;
        }
        return true;
    }
    // float64
    if (b == 0xCB) { pos += 9; return pos <= len; }
    // str8
    if (b == 0xD9) {
        if (pos + 2 > len) return false;
        size_t slen = data[pos + 1];
        pos += 2 + slen;
        return pos <= len;
    }
    // str16
    if (b == 0xDA) {
        if (pos + 3 > len) return false;
        size_t slen = ((size_t)data[pos + 1] << 8) | data[pos + 2];
        pos += 3 + slen;
        return pos <= len;
    }
    // positive fixint
    if ((b & 0x80) == 0x00) { pos++; return true; }
    // negative fixint
    if ((b & 0xE0) == 0xE0) { pos++; return true; }
    // nil
    if (b == 0xC0) { pos++; return true; }
    // bool
    if (b == 0xC2 || b == 0xC3) { pos++; return true; }
    // uint8
    if (b == 0xCC) { pos += 2; return pos <= len; }
    // uint16
    if (b == 0xCD) { pos += 3; return pos <= len; }
    // uint32
    if (b == 0xCE) { pos += 5; return pos <= len; }
    // int8
    if (b == 0xD0) { pos += 2; return pos <= len; }
    // int16
    if (b == 0xD1) { pos += 3; return pos <= len; }
    // int32
    if (b == 0xD2) { pos += 5; return pos <= len; }
    // float32
    if (b == 0xCA) { pos += 5; return pos <= len; }
    // bin8
    if (b == 0xC4) {
        if (pos + 2 > len) return false;
        size_t blen = data[pos + 1];
        pos += 2 + blen;
        return pos <= len;
    }
    // bin16
    if (b == 0xC5) {
        if (pos + 3 > len) return false;
        size_t blen = ((size_t)data[pos + 1] << 8) | data[pos + 2];
        pos += 3 + blen;
        return pos <= len;
    }
    // bin32
    if (b == 0xC6) {
        if (pos + 5 > len) return false;
        size_t blen = ((size_t)data[pos + 1] << 24) | ((size_t)data[pos + 2] << 16) |
                      ((size_t)data[pos + 3] << 8) | data[pos + 4];
        pos += 5 + blen;
        return pos <= len;
    }
    // str32
    if (b == 0xDB) {
        if (pos + 5 > len) return false;
        size_t slen = ((size_t)data[pos + 1] << 24) | ((size_t)data[pos + 2] << 16) |
                      ((size_t)data[pos + 3] << 8) | data[pos + 4];
        pos += 5 + slen;
        return pos <= len;
    }
    // uint64
    if (b == 0xCF) { pos += 9; return pos <= len; }
    // int64
    if (b == 0xD3) { pos += 9; return pos <= len; }
    // array16
    if (b == 0xDC) {
        if (pos + 3 > len) return false;
        size_t count = ((size_t)data[pos + 1] << 8) | data[pos + 2];
        pos += 3;
        for (size_t i = 0; i < count; i++) {
            if (!mpSkipValue(data, len, pos)) return false;
        }
        return true;
    }
    // map16
    if (b == 0xDE) {
        if (pos + 3 > len) return false;
        size_t count = ((size_t)data[pos + 1] << 8) | data[pos + 2];
        pos += 3;
        for (size_t i = 0; i < count * 2; i++) {
            if (!mpSkipValue(data, len, pos)) return false;
        }
        return true;
    }

    // Unknown type — cannot safely skip
    Serial.printf("[LXMF] Unknown MsgPack type: 0x%02X at pos %d\n", b, (int)pos);
    return false;
}

// =============================================================================
// LXMFMessage implementation
// =============================================================================

std::vector<uint8_t> LXMFMessage::packContent(double timestamp, const std::string& content, const std::string& title) {
    std::vector<uint8_t> buf;
    buf.reserve(32 + content.size() + title.size());

    // fixarray(4): [timestamp, title, content, fields] — LXMF spec order
    // Title and content packed as bin type (LXMF stores them as bytes)
    buf.push_back(0x94);
    mpPackFloat64(buf, timestamp);
    mpPackBin(buf, title);
    mpPackBin(buf, content);
    // empty fixmap for fields
    buf.push_back(0x80);

    return buf;
}

std::vector<uint8_t> LXMFMessage::packFull(const RNS::Identity& signingIdentity) const {
    // Pack content
    std::vector<uint8_t> packed = packContent(timestamp, content, title);

    // Validate hashes
    if (sourceHash.size() < 16 || destHash.size() < 16) {
        Serial.println("[LXMF] ERROR: sourceHash or destHash too short, cannot pack");
        return {};
    }

    // hashed_part = dest_hash(16) + source_hash(16) + packed_content
    std::vector<uint8_t> hashedPart;
    hashedPart.reserve(16 + 16 + packed.size());
    hashedPart.insert(hashedPart.end(), destHash.data(), destHash.data() + 16);
    hashedPart.insert(hashedPart.end(), sourceHash.data(), sourceHash.data() + 16);
    hashedPart.insert(hashedPart.end(), packed.begin(), packed.end());

    // message_hash = full_hash(hashed_part)
    RNS::Bytes hashedPartBytes(hashedPart.data(), hashedPart.size());
    RNS::Bytes messageHash = RNS::Identity::full_hash(hashedPartBytes);

    // signed_part = hashed_part + message_hash
    std::vector<uint8_t> signedPart;
    signedPart.reserve(hashedPart.size() + messageHash.size());
    signedPart.insert(signedPart.end(), hashedPart.begin(), hashedPart.end());
    signedPart.insert(signedPart.end(), messageHash.data(), messageHash.data() + messageHash.size());

    // Sign
    RNS::Bytes signedPartBytes(signedPart.data(), signedPart.size());
    RNS::Bytes sig = signingIdentity.sign(signedPartBytes);
    if (sig.size() < 64) {
        Serial.println("[LXMF] ERROR: signing failed, cannot pack");
        return {};
    }

    // Wire format (opportunistic): source_hash(16) + signature(64) + packed_content
    std::vector<uint8_t> payload;
    payload.reserve(16 + 64 + packed.size());
    payload.insert(payload.end(), sourceHash.data(), sourceHash.data() + 16);
    payload.insert(payload.end(), sig.data(), sig.data() + 64);
    payload.insert(payload.end(), packed.begin(), packed.end());

    return payload;
}

bool LXMFMessage::unpackFull(const uint8_t* data, size_t len, LXMFMessage& msg) {
    // Minimum: 16 (source) + 64 (sig) + 1 (array header) + 9 (timestamp) + 1 (empty str) + 1 (empty str) + 1 (empty map) = 93
    if (len < 93) {
        Serial.printf("[LXMF] Payload too short: %d bytes\n", (int)len);
        return false;
    }

    // Wire format: source_hash(16) + signature(64) + packed_content
    msg.sourceHash = RNS::Bytes(data, 16);
    msg.signature = RNS::Bytes(data + 16, 64);

    // Parse MsgPack content (after source hash and signature)
    const uint8_t* content = data + 16 + 64;
    size_t contentLen = len - 16 - 64;
    size_t pos = 0;

    // Expect fixarray(4) or fixarray(3)
    if (pos >= contentLen) return false;
    uint8_t arrHeader = content[pos];
    if ((arrHeader & 0xF0) != 0x90) {
        Serial.printf("[LXMF] Expected array, got 0x%02X\n", arrHeader);
        return false;
    }
    size_t arrLen = arrHeader & 0x0F;
    if (arrLen < 3) {
        Serial.printf("[LXMF] Array too short: %d\n", (int)arrLen);
        return false;
    }
    pos++;

    // [0] timestamp (float64)
    if (!mpReadFloat64(content, contentLen, pos, msg.timestamp)) {
        Serial.println("[LXMF] Failed to read timestamp");
        return false;
    }

    // [1] title (string) — LXMF spec order
    if (!mpReadString(content, contentLen, pos, msg.title)) {
        Serial.println("[LXMF] Failed to read title");
        return false;
    }

    // [2] content (string)
    if (!mpReadString(content, contentLen, pos, msg.content)) {
        Serial.println("[LXMF] Failed to read content");
        return false;
    }

    // [3] fields (map) — skip if present
    if (arrLen >= 4 && pos < contentLen) {
        mpSkipValue(content, contentLen, pos);
    }

    // Generate message ID: full_hash(dest_hash + source_hash + packed_content)
    // Note: dest_hash is set by the caller (from the packet destination)
    RNS::Bytes fullPayload(data, len);
    msg.messageId = RNS::Identity::full_hash(fullPayload);

    msg.incoming = true;
    msg.status = LXMFStatus::DELIVERED;

    return true;
}

const char* LXMFMessage::statusStr() const {
    switch (status) {
        case LXMFStatus::DRAFT:     return "draft";
        case LXMFStatus::QUEUED:    return "queued";
        case LXMFStatus::SENDING:   return "sending";
        case LXMFStatus::SENT:      return "sent";
        case LXMFStatus::DELIVERED: return "delivered";
        case LXMFStatus::FAILED:    return "failed";
        default:                    return "?";
    }
}
