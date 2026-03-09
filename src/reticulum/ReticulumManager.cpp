#include "ReticulumManager.h"
#include "config/Config.h"
#include <LittleFS.h>
#include <Preferences.h>

// =============================================================================
// LittleFS Filesystem Implementation for microReticulum
// =============================================================================

bool LittleFSFileSystem::init() {
    return true;  // LittleFS already initialized by FlashStore
}

bool LittleFSFileSystem::file_exists(const char* file_path) {
    return LittleFS.exists(file_path);
}

size_t LittleFSFileSystem::read_file(const char* file_path, RNS::Bytes& data) {
    File f = LittleFS.open(file_path, "r");
    if (!f) return 0;
    size_t size = f.size();
    data = RNS::Bytes(size);
    f.readBytes((char*)data.writable(size), size);
    f.close();
    return size;
}

size_t LittleFSFileSystem::write_file(const char* file_path, const RNS::Bytes& data) {
    // Ensure parent directory exists
    String path = String(file_path);
    int lastSlash = path.lastIndexOf('/');
    if (lastSlash > 0) {
        String dir = path.substring(0, lastSlash);
        if (!LittleFS.exists(dir.c_str())) {
            LittleFS.mkdir(dir.c_str());
        }
    }

    File f = LittleFS.open(file_path, "w");
    if (!f) return 0;
    size_t written = f.write(data.data(), data.size());
    f.close();
    return written;
}

RNS::FileStream LittleFSFileSystem::open_file(const char* file_path, RNS::FileStream::MODE file_mode) {
    return {RNS::Type::NONE};
}

bool LittleFSFileSystem::remove_file(const char* file_path) {
    return LittleFS.remove(file_path);
}

bool LittleFSFileSystem::rename_file(const char* from, const char* to) {
    return LittleFS.rename(from, to);
}

bool LittleFSFileSystem::directory_exists(const char* directory_path) {
    return LittleFS.exists(directory_path);
}

bool LittleFSFileSystem::create_directory(const char* directory_path) {
    return LittleFS.mkdir(directory_path);
}

bool LittleFSFileSystem::remove_directory(const char* directory_path) {
    return LittleFS.rmdir(directory_path);
}

std::list<std::string> LittleFSFileSystem::list_directory(const char* directory_path) {
    std::list<std::string> entries;
    File dir = LittleFS.open(directory_path);
    if (!dir || !dir.isDirectory()) return entries;
    File f = dir.openNextFile();
    while (f) {
        entries.push_back(f.name());
        f = dir.openNextFile();
    }
    return entries;
}

size_t LittleFSFileSystem::storage_size() {
    return LittleFS.totalBytes();
}

size_t LittleFSFileSystem::storage_available() {
    return LittleFS.totalBytes() - LittleFS.usedBytes();
}

// =============================================================================
// ReticulumManager
// =============================================================================

bool ReticulumManager::begin(SX1262* radio, FlashStore* flash) {
    _flash = flash;

    // Register filesystem with microReticulum (required — library throws if missing)
    LittleFSFileSystem* fsImpl = new LittleFSFileSystem();
    RNS::FileSystem fs(fsImpl);
    fs.init();
    RNS::Utilities::OS::register_filesystem(fs);
    Serial.println("[RNS] Filesystem registered");

    // Create and register LoRa interface
    _loraImpl = new LoRaInterface(radio, "LoRa.915");
    _loraIface = _loraImpl;
    _loraIface.mode(RNS::Type::Interface::MODE_GATEWAY);
    RNS::Transport::register_interface(_loraIface);
    if (!_loraImpl->start()) {
        Serial.println("[RNS] WARNING: LoRa interface failed to start — radio offline");
    }
    Serial.println("[RNS] LoRa interface registered");

    // Create Reticulum instance and enable transport
    _reticulum = RNS::Reticulum();
    RNS::Reticulum::transport_enabled(true);
    RNS::Reticulum::probe_destination_enabled(true);
    // Cap table sizes to prevent OOM on memory-constrained ESP32 (320KB RAM)
    RNS::Transport::path_table_maxsize(16);
    RNS::Transport::announce_table_maxsize(16);
    RNS::Transport::hashlist_maxsize(32);
    RNS::Transport::max_pr_tags(8);
    RNS::Identity::known_destinations_maxsize(16);
    _reticulum.start();
    Serial.println("[RNS] Reticulum started (Transport Node)");

    // Load persisted known destinations so Identity::recall() works
    // immediately after reboot for previously-seen nodes.
    RNS::Identity::load_known_destinations();

    // Load or create identity
    if (!loadOrCreateIdentity()) {
        Serial.println("[RNS] ERROR: Identity creation failed!");
        return false;
    }

    // Create LXMF delivery destination
    _destination = RNS::Destination(
        _identity,
        RNS::Type::Destination::IN,
        RNS::Type::Destination::SINGLE,
        "lxmf",
        "delivery"
    );
    _destination.set_proof_strategy(RNS::Type::Destination::PROVE_ALL);
    _destination.accepts_links(true);
    Serial.printf("[RNS] Destination: %s\n", _destination.hash().toHex().c_str());

    _transportActive = true;
    Serial.println("[RNS] Transport node active");
    return true;
}

bool ReticulumManager::loadOrCreateIdentity() {
    // Tier 1: Flash (LittleFS)
    if (_flash->exists(PATH_IDENTITY)) {
        uint8_t keyBuf[128];
        size_t keyLen = 0;
        if (_flash->readFile(PATH_IDENTITY, keyBuf, sizeof(keyBuf), keyLen) && keyLen > 0) {
            RNS::Bytes keyData(keyBuf, keyLen);
            _identity = RNS::Identity(false);
            if (_identity.load_private_key(keyData)) {
                Serial.printf("[RNS] Identity loaded from flash: %s\n", _identity.hexhash().c_str());
                saveIdentityToAll(keyData);
                return true;
            }
        }
        Serial.println("[RNS] Failed to load identity from flash");
    }

    // Tier 2: NVS (ESP32 Preferences — always available, higher trust than SD)
    {
        Preferences prefs;
        if (prefs.begin("ratcom_id", true)) {
            size_t keyLen = prefs.getBytesLength("privkey");
            if (keyLen > 0 && keyLen <= 128) {
                uint8_t keyBuf[128];
                prefs.getBytes("privkey", keyBuf, keyLen);
                prefs.end();
                RNS::Bytes keyData(keyBuf, keyLen);
                _identity = RNS::Identity(false);
                if (_identity.load_private_key(keyData)) {
                    Serial.printf("[RNS] Identity restored from NVS: %s\n", _identity.hexhash().c_str());
                    saveIdentityToAll(keyData);
                    return true;
                }
            } else {
                prefs.end();
            }
        }
    }

    // Tier 3: SD card (lowest trust — may contain another device's identity)
    if (_sd && _sd->isReady() && _sd->exists(SD_PATH_IDENTITY)) {
        uint8_t keyBuf[128];
        size_t keyLen = 0;
        if (_sd->readFile(SD_PATH_IDENTITY, keyBuf, sizeof(keyBuf), keyLen) && keyLen > 0) {
            RNS::Bytes keyData(keyBuf, keyLen);
            _identity = RNS::Identity(false);
            if (_identity.load_private_key(keyData)) {
                Serial.printf("[RNS] Identity restored from SD: %s\n", _identity.hexhash().c_str());
                saveIdentityToAll(keyData);
                return true;
            }
        }
        Serial.println("[RNS] SD identity exists but failed to load");
    }

    // No identity found anywhere — create new
    _identity = RNS::Identity();
    Serial.printf("[RNS] New identity created: %s\n", _identity.hexhash().c_str());

    RNS::Bytes privKey = _identity.get_private_key();
    if (privKey.size() > 0) {
        saveIdentityToAll(privKey);
    }
    return true;
}

void ReticulumManager::saveIdentityToAll(const RNS::Bytes& keyData) {
    // Flash
    _flash->writeAtomic(PATH_IDENTITY, keyData.data(), keyData.size());
    // SD
    if (_sd && _sd->isReady()) {
        _sd->ensureDir("/ratcom/identity");
        _sd->writeAtomic(SD_PATH_IDENTITY, keyData.data(), keyData.size());
    }
    // NVS (always available)
    Preferences prefs;
    if (prefs.begin("ratcom_id", false)) {
        prefs.putBytes("privkey", keyData.data(), keyData.size());
        prefs.end();
        Serial.println("[RNS] Identity saved to NVS");
    }
}

void ReticulumManager::loop() {
    if (!_transportActive) return;

    _reticulum.loop();
    if (_loraImpl) {
        _loraImpl->loop();
    }
}

String ReticulumManager::identityHash() const {
    if (!_identity) return "unknown";
    std::string hex = _identity.hexhash();
    if (hex.length() >= 12) {
        return String((hex.substr(0, 4) + ":" + hex.substr(4, 4) + ":" + hex.substr(8, 4)).c_str());
    }
    return String(hex.c_str());
}

String ReticulumManager::destinationHashStr() const {
    if (!_destination) return "unknown";
    std::string hex = _destination.hash().toHex();
    if (hex.length() >= 12) {
        return String((hex.substr(0, 4) + ":" + hex.substr(4, 4) + ":" + hex.substr(8, 4)).c_str());
    }
    return String(hex.c_str());
}

size_t ReticulumManager::pathCount() const {
    return _reticulum.get_path_table().size();
}

size_t ReticulumManager::linkCount() const {
    return _reticulum.get_link_count();
}

void ReticulumManager::announce(const RNS::Bytes& appData) {
    if (!_transportActive) return;
    Serial.printf("[TX-DBG] dest_hash:     %s\n", _destination.hash().toHex().c_str());
    Serial.printf("[TX-DBG] identity_hash: %s\n", _identity.hexhash().c_str());
    Serial.printf("[TX-DBG] public_key:    %s\n", _identity.get_public_key().toHex().c_str());
    RNS::Bytes nh = RNS::Identity::full_hash(RNS::Bytes("lxmf.delivery")).left(10);
    Serial.printf("[TX-DBG] name_hash:     %s\n", nh.toHex().c_str());
    RNS::Bytes hm = nh + _identity.hash();
    Serial.printf("[TX-DBG] hash_material: %s\n", hm.toHex().c_str());
    RNS::Bytes eh = RNS::Identity::full_hash(hm).left(16);
    Serial.printf("[TX-DBG] recomputed:    %s\n", eh.toHex().c_str());
    _destination.announce(appData);
    _lastAnnounceTime = millis();
    Serial.println("[RNS] Announce sent");
}
