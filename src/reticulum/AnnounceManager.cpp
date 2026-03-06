#include "AnnounceManager.h"
#include "config/Config.h"
#include "storage/SDStore.h"
#include "storage/FlashStore.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

// Try to extract display name from MsgPack-encoded app_data.
// Sideband format: MsgPack array where element[0] is display name string.
static std::string extractMsgPackName(const uint8_t* data, size_t len) {
    if (len < 2) return "";
    uint8_t b = data[0];
    size_t pos = 0;

    // fixarray (0x90-0x9F): element count in low nibble
    if ((b & 0xF0) == 0x90) {
        if ((b & 0x0F) == 0) return "";
        pos = 1;
    }
    // array16 (0xDC)
    else if (b == 0xDC && len >= 3) {
        pos = 3;
    }
    else return "";  // Not a MsgPack array

    if (pos >= len) return "";
    b = data[pos];
    size_t slen = 0;

    // fixstr (0xA0-0xBF)
    if ((b & 0xE0) == 0xA0) { slen = b & 0x1F; pos++; }
    // str8 (0xD9)
    else if (b == 0xD9 && pos + 1 < len) { slen = data[pos+1]; pos += 2; }
    // str16 (0xDA)
    else if (b == 0xDA && pos + 2 < len) {
        slen = ((size_t)data[pos+1] << 8) | data[pos+2]; pos += 3;
    }
    else return "";  // First element not a string

    if (pos + slen > len) return "";
    return std::string((const char*)&data[pos], slen);
}

// Tighter character filter — only safe displayable characters
static std::string sanitizeName(const std::string& raw, size_t maxLen = 16) {
    std::string clean;
    clean.reserve(std::min(raw.size(), maxLen));
    for (char c : raw) {
        if (clean.size() >= maxLen) break;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') ||
            c == ' ' || c == '-' || c == '_' || c == '.' || c == '\'') {
            clean += c;
        }
    }
    // Trim leading/trailing spaces
    size_t start = clean.find_first_not_of(' ');
    if (start == std::string::npos) return "";
    size_t end = clean.find_last_not_of(' ');
    return clean.substr(start, end - start + 1);
}

AnnounceManager::AnnounceManager(const char* aspectFilter)
    : RNS::AnnounceHandler(aspectFilter)
{
}

void AnnounceManager::setStorage(SDStore* sd, FlashStore* flash) {
    _sd = sd;
    _flash = flash;
}

void AnnounceManager::received_announce(
    const RNS::Bytes& destination_hash,
    const RNS::Identity& announced_identity,
    const RNS::Bytes& app_data)
{
    Serial.printf("[ANNOUNCE] From: %s", destination_hash.toHex().c_str());

    // Extract display name from app_data
    std::string name;
    if (app_data.size() > 0) {
        // Try MsgPack first (Sideband format)
        std::string rawName = extractMsgPackName(app_data.data(), app_data.size());
        if (rawName.empty()) {
            rawName = app_data.toString();  // Fall back to raw string
        }
        name = sanitizeName(rawName);
        Serial.printf(" name=\"%s\"", name.c_str());
    }
    Serial.println();

    // Check if already known
    for (auto& node : _nodes) {
        if (node.hash == destination_hash) {
            // Update existing (name already sanitized above)
            if (!name.empty()) node.name = name;
            node.lastSeen = millis();
            node.hops = RNS::Transport::hops_to(destination_hash);
            // Re-save if this is a persisted contact
            if (node.saved) {
                saveContact(node);
            }
            return;
        }
    }

    // Add new node
    if ((int)_nodes.size() >= MAX_NODES) {
        evictStale();
        // If still full, remove oldest unsaved node
        if ((int)_nodes.size() >= MAX_NODES) {
            unsigned long oldest = ULONG_MAX;
            int oldestIdx = -1;
            for (int i = 0; i < (int)_nodes.size(); i++) {
                if (!_nodes[i].saved && _nodes[i].lastSeen < oldest) {
                    oldest = _nodes[i].lastSeen;
                    oldestIdx = i;
                }
            }
            if (oldestIdx >= 0) {
                _nodes.erase(_nodes.begin() + oldestIdx);
            }
            // If all nodes are saved, can't evict — skip adding new node
        }
    }

    // If still at capacity (all saved), drop announce
    if ((int)_nodes.size() >= MAX_NODES) {
        Serial.println("[ANNOUNCE] Node list full (all saved), dropping announce");
        return;
    }

    DiscoveredNode node;
    node.hash = destination_hash;
    node.name = name.empty() ? destination_hash.toHex().substr(0, 12) : name;
    node.lastSeen = millis();
    node.hops = RNS::Transport::hops_to(destination_hash);
    _nodes.push_back(node);

    Serial.printf("[ANNOUNCE] Added node %s (%d total)\n",
                  node.name.c_str(), (int)_nodes.size());
}

const DiscoveredNode* AnnounceManager::findNode(const RNS::Bytes& hash) const {
    for (const auto& node : _nodes) {
        if (node.hash == hash) return &node;
    }
    return nullptr;
}

void AnnounceManager::addManualContact(const std::string& hexHash, const std::string& name) {
    RNS::Bytes hash;
    hash.assignHex(hexHash.c_str());

    std::string safeName = sanitizeName(name);

    // Check duplicate
    for (auto& node : _nodes) {
        if (node.hash == hash) {
            if (!safeName.empty()) node.name = safeName;
            node.saved = true;
            saveContact(node);
            return;
        }
    }

    DiscoveredNode node;
    node.hash = hash;
    node.name = safeName.empty() ? hexHash.substr(0, 12) : safeName;
    node.lastSeen = millis();
    node.saved = true;
    _nodes.push_back(node);
    saveContact(node);
}

void AnnounceManager::saveNode(const std::string& hexHash) {
    RNS::Bytes hash;
    hash.assignHex(hexHash.c_str());
    for (auto& node : _nodes) {
        if (node.hash == hash) {
            node.saved = true;
            saveContact(node);
            Serial.printf("[ANNOUNCE] Saved contact: %s\n", node.name.c_str());
            return;
        }
    }
}

void AnnounceManager::unsaveNode(const std::string& hexHash) {
    RNS::Bytes hash;
    hash.assignHex(hexHash.c_str());
    for (auto& node : _nodes) {
        if (node.hash == hash) {
            node.saved = false;
            removeContact(hexHash);
            Serial.printf("[ANNOUNCE] Removed contact: %s\n", node.name.c_str());
            return;
        }
    }
}

void AnnounceManager::evictStale(unsigned long maxAgeMs) {
    unsigned long now = millis();
    _nodes.erase(
        std::remove_if(_nodes.begin(), _nodes.end(),
            [now, maxAgeMs](const DiscoveredNode& n) {
                return !n.saved && (now - n.lastSeen > maxAgeMs);
            }),
        _nodes.end());
}

void AnnounceManager::saveContact(const DiscoveredNode& node) {
    std::string hexHash = node.hash.toHex();

    JsonDocument doc;
    doc["hash"] = hexHash;
    doc["name"] = node.name;
    doc["rssi"] = node.rssi;
    doc["snr"] = node.snr;
    doc["hops"] = node.hops;
    doc["lastSeen"] = node.lastSeen;

    String json;
    serializeJson(doc, json);

    String filename = hexHash.substr(0, 16).c_str();
    filename += ".json";

    bool sdOk = false, flashOk = false;

    // Write to SD (primary)
    if (_sd && _sd->isReady()) {
        _sd->ensureDir(SD_PATH_CONTACTS);
        String sdPath = String(SD_PATH_CONTACTS) + filename;
        sdOk = _sd->writeString(sdPath.c_str(), json);
        if (!sdOk) {
            // Atomic rename failed — try direct write
            sdOk = _sd->writeDirect(sdPath.c_str(),
                                     (const uint8_t*)json.c_str(), json.length());
        }
        Serial.printf("[CONTACT] SD write %s: %s\n",
                      sdOk ? "OK" : "FAILED", sdPath.c_str());
    } else {
        Serial.println("[CONTACT] SD not ready, skipping");
    }

    // Write to flash (fallback)
    if (_flash && _flash->isReady()) {
        _flash->ensureDir(PATH_CONTACTS);
        String flashPath = String(PATH_CONTACTS) + filename;
        flashOk = _flash->writeString(flashPath.c_str(), json);
        if (!flashOk) {
            flashOk = _flash->writeDirect(flashPath.c_str(),
                                           (const uint8_t*)json.c_str(), json.length());
        }
        Serial.printf("[CONTACT] Flash write %s: %s\n",
                      flashOk ? "OK" : "FAILED", flashPath.c_str());
    }

    if (!sdOk && !flashOk) {
        Serial.printf("[CONTACT] WARNING: Failed to persist contact %s!\n",
                      hexHash.substr(0, 8).c_str());
    }
}

void AnnounceManager::removeContact(const std::string& hexHash) {
    String filename = hexHash.substr(0, 16).c_str();
    filename += ".json";

    if (_sd && _sd->isReady()) {
        String sdPath = String(SD_PATH_CONTACTS) + filename;
        _sd->remove(sdPath.c_str());
        // Also clean up atomic write artifacts
        _sd->remove((sdPath + ".tmp").c_str());
        _sd->remove((sdPath + ".bak").c_str());
    }
    if (_flash && _flash->isReady()) {
        String flashPath = String(PATH_CONTACTS) + filename;
        _flash->remove(flashPath.c_str());
        _flash->remove((flashPath + ".tmp").c_str());
        _flash->remove((flashPath + ".bak").c_str());
    }
    Serial.printf("[CONTACT] Removed contact %s\n", hexHash.substr(0, 8).c_str());
}

void AnnounceManager::loadContacts() {
    int loaded = 0;

    auto loadFromDir = [&](File& dir, const char* source) {
        int filesFound = 0;
        File entry = dir.openNextFile();
        while (entry) {
            String entryName = entry.name();
            if (!entry.isDirectory() && entryName.endsWith(".json")) {
                filesFound++;
                size_t size = entry.size();
                if (size > 0 && size < 2048) {
                    String json = entry.readString();

                    JsonDocument doc;
                    if (!deserializeJson(doc, json)) {
                        std::string hexHash = doc["hash"] | "";
                        if (!hexHash.empty()) {
                            // Check if already loaded (avoid duplicates)
                            RNS::Bytes hash;
                            hash.assignHex(hexHash.c_str());
                            bool dup = false;
                            for (auto& n : _nodes) {
                                if (n.hash == hash) { dup = true; break; }
                            }
                            if (!dup) {
                                DiscoveredNode node;
                                node.hash = hash;
                                node.name = sanitizeName(doc["name"] | "");
                                if (node.name.empty()) node.name = hexHash.substr(0, 12);
                                node.rssi = doc["rssi"] | 0;
                                node.snr = doc["snr"] | 0.0f;
                                node.hops = doc["hops"] | 0;
                                node.lastSeen = doc["lastSeen"] | (unsigned long)millis();
                                node.saved = true;
                                _nodes.push_back(node);
                                loaded++;
                                Serial.printf("[CONTACT] Loaded from %s: %s (%s)\n",
                                              source, node.name.c_str(),
                                              hexHash.substr(0, 8).c_str());
                            }
                        }
                    } else {
                        Serial.printf("[CONTACT] JSON parse failed: %s\n", entryName.c_str());
                    }
                }
            }
            entry = dir.openNextFile();
        }
        Serial.printf("[CONTACT] %s: %d json files found\n", source, filesFound);
    };

    // Load from SD first (primary)
    if (_sd && _sd->isReady()) {
        File dir = _sd->openDir(SD_PATH_CONTACTS);
        if (dir && dir.isDirectory()) {
            loadFromDir(dir, "SD");
        } else {
            Serial.println("[CONTACT] SD contacts dir not found");
        }
    } else {
        Serial.println("[CONTACT] SD not ready for contact loading");
    }

    // Load from flash (any contacts not already on SD)
    if (_flash && _flash->isReady()) {
        File dir = LittleFS.open(PATH_CONTACTS);
        if (dir && dir.isDirectory()) {
            loadFromDir(dir, "Flash");
        } else {
            Serial.println("[CONTACT] Flash contacts dir not found");
        }
    }

    Serial.printf("[CONTACT] Total loaded: %d saved contacts\n", loaded);
}

void AnnounceManager::saveContacts() {
    for (const auto& node : _nodes) {
        if (node.saved) {
            saveContact(node);
        }
    }
}
