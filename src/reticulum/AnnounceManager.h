#pragma once

#include <Transport.h>
#include <Identity.h>
#include <Bytes.h>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>
#include "config/Config.h"

class SDStore;
class FlashStore;

struct DiscoveredNode {
    RNS::Bytes hash;
    std::string name;
    std::string identityHex;
    int rssi = 0;
    float snr = 0;
    uint8_t hops = 0;
    unsigned long lastSeen = 0;
    bool saved = false;  // Persisted as contact
};

class AnnounceManager : public RNS::AnnounceHandler {
public:
    AnnounceManager(const char* aspectFilter = nullptr);
    virtual ~AnnounceManager() = default;

    virtual void received_announce(
        const RNS::Bytes& destination_hash,
        const RNS::Identity& announced_identity,
        const RNS::Bytes& app_data) override;

    // Storage for contact persistence
    void setStorage(SDStore* sd, FlashStore* flash);
    void setLocalDestHash(const RNS::Bytes& hash) { _localDestHash = hash; }

    // Save/load persisted contacts
    void saveContacts();
    void loadContacts();
    void loop();  // Call from main loop — handles deferred saves

    // Name cache: persists hash→name mappings so names survive reboots
    std::string lookupName(const std::string& hexHash) const;
    void saveNameCache();
    void loadNameCache();

    // Node list access
    const std::vector<DiscoveredNode>& nodes() const { return _nodes; }
    int nodeCount() const { return _nodes.size(); }

    // Find node by hash
    const DiscoveredNode* findNode(const RNS::Bytes& hash) const;
    const DiscoveredNode* findNodeByHex(const std::string& hexHash) const;

    // Manual contact add
    void addManualContact(const std::string& hexHash, const std::string& name);

    // Save/unsave a discovered node as contact by hex hash
    void saveNode(const std::string& hexHash);
    void unsaveNode(const std::string& hexHash);

    // Evict old entries
    void evictStale(unsigned long maxAgeMs = 3600000);  // 1 hour default

    // Clear all nodes and state (used during first-boot data wipe)
    void clearAll();

    void rebuildIndex();

private:
    void saveContact(const DiscoveredNode& node);
    void removeContact(const std::string& hexHash);

    std::vector<DiscoveredNode> _nodes;
    std::unordered_map<std::string, int> _hashIndex;  // raw hash bytes → _nodes index

    static std::string makeKey(const RNS::Bytes& hash) {
        return std::string((const char*)hash.data(), hash.size());
    }

    SDStore* _sd = nullptr;
    FlashStore* _flash = nullptr;
    RNS::Bytes _localDestHash;
    bool _contactsDirty = false;
    bool _nameCacheDirty = false;
    unsigned long _lastContactSave = 0;
    std::map<std::string, std::string> _nameCache;  // hexHash → displayName
    unsigned long _globalAnnounceWindowStart = 0;
    unsigned int _globalAnnounceCount = 0;
    static constexpr unsigned int MAX_GLOBAL_ANNOUNCES_PER_SEC = 8;
    static constexpr int MAX_NODES = RATPUTER_MAX_NODES;
    static constexpr int MAX_NAME_CACHE = 200;  // Cap name cache to prevent heap exhaustion
    static constexpr unsigned long CONTACT_SAVE_INTERVAL_MS = 30000;
    static constexpr unsigned long ANNOUNCE_MIN_INTERVAL_MS = 200;  // Rate-limit announce processing
};
