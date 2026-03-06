#include "SDStore.h"
#include "config/Config.h"

bool SDStore::begin(SPIClass* spi, int csPin) {
    if (!spi) return false;

    // Attach to existing HSPI bus (already started by radio)
    if (!SD.begin(csPin, *spi)) {
        Serial.println("[SD] Card not detected or mount failed");
        _ready = false;
        return false;
    }

    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        Serial.println("[SD] No card inserted");
        _ready = false;
        return false;
    }

    const char* typeStr = "UNKNOWN";
    if (cardType == CARD_MMC)  typeStr = "MMC";
    if (cardType == CARD_SD)   typeStr = "SD";
    if (cardType == CARD_SDHC) typeStr = "SDHC";

    _ready = true;
    Serial.printf("[SD] %s card ready, total=%llu MB, used=%llu MB\n",
                  typeStr, totalBytes() / (1024 * 1024), usedBytes() / (1024 * 1024));
    return true;
}

void SDStore::end() {
    SD.end();
    _ready = false;
}

uint64_t SDStore::totalBytes() const {
    if (!_ready) return 0;
    return SD.totalBytes();
}

uint64_t SDStore::usedBytes() const {
    if (!_ready) return 0;
    return SD.usedBytes();
}

bool SDStore::ensureDir(const char* path) {
    if (!_ready) return false;
    if (SD.exists(path)) return true;
    return SD.mkdir(path);
}

bool SDStore::exists(const char* path) {
    if (!_ready) return false;
    return SD.exists(path);
}

bool SDStore::remove(const char* path) {
    if (!_ready) return false;
    return SD.remove(path);
}

File SDStore::openDir(const char* path) {
    if (!_ready) return File();
    return SD.open(path);
}

bool SDStore::removeDir(const char* path) {
    if (!_ready) return false;
    return SD.rmdir(path);
}

bool SDStore::readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& bytesRead) {
    bytesRead = 0;
    if (!_ready) return false;
    File f = SD.open(path, FILE_READ);
    if (!f) return false;
    size_t size = f.size();
    if (size > maxLen) { f.close(); return false; }
    bytesRead = f.read(buffer, size);
    f.close();
    return bytesRead == size;
}

bool SDStore::writeAtomic(const char* path, const uint8_t* data, size_t len) {
    if (!_ready) return false;

    String tmpPath = String(path) + ".tmp";
    String bakPath = String(path) + ".bak";

    // Step 1: Write to .tmp
    File f = SD.open(tmpPath.c_str(), FILE_WRITE);
    if (!f) {
        Serial.printf("[SD] Failed to open %s for write\n", tmpPath.c_str());
        return false;
    }
    size_t written = f.write(data, len);
    f.close();
    if (written != len) {
        SD.remove(tmpPath.c_str());
        return false;
    }

    // Step 2: Verify .tmp by reading back size
    File verify = SD.open(tmpPath.c_str(), FILE_READ);
    if (!verify || verify.size() != len) {
        if (verify) verify.close();
        SD.remove(tmpPath.c_str());
        return false;
    }
    verify.close();

    // Step 3: Rename current to .bak (if exists)
    if (SD.exists(path)) {
        SD.remove(bakPath.c_str());
        SD.rename(path, bakPath.c_str());
    }

    // Step 4: Rename .tmp to primary
    if (!SD.rename(tmpPath.c_str(), path)) {
        // Restore from backup on failure
        if (SD.exists(bakPath.c_str())) {
            SD.rename(bakPath.c_str(), path);
        }
        return false;
    }

    return true;
}

bool SDStore::writeString(const char* path, const String& data) {
    return writeAtomic(path, (const uint8_t*)data.c_str(), data.length());
}

bool SDStore::writeDirect(const char* path, const uint8_t* data, size_t len) {
    if (!_ready) return false;
    File f = SD.open(path, FILE_WRITE);
    if (!f) return false;
    size_t written = f.write(data, len);
    f.flush();
    f.close();
    return written == len;
}

String SDStore::readString(const char* path) {
    if (!_ready) return "";

    File f = SD.open(path, FILE_READ);
    if (!f) {
        // Try backup
        String bakPath = String(path) + ".bak";
        f = SD.open(bakPath.c_str(), FILE_READ);
        if (!f) return "";
        Serial.printf("[SD] Restored from backup: %s\n", path);
    }

    String result = f.readString();
    f.close();
    return result;
}

bool SDStore::wipeRatputer() {
    if (!_ready) return false;
    Serial.println("[SD] Wiping /ratputer/ ...");
    wipeDir("/ratputer/messages");
    wipeDir("/ratputer/contacts");
    wipeDir("/ratputer/identity");
    wipeDir("/ratputer/config");
    SD.rmdir("/ratputer");
    Serial.println("[SD] Wipe complete, recreating dirs...");
    return formatForRatputer();
}

void SDStore::wipeDir(const char* path) {
    File dir = SD.open(path);
    if (!dir || !dir.isDirectory()) return;
    File entry = dir.openNextFile();
    while (entry) {
        String fullPath = String(path) + "/" + entry.name();
        if (entry.isDirectory()) {
            wipeDir(fullPath.c_str());
            SD.rmdir(fullPath.c_str());
        } else {
            SD.remove(fullPath.c_str());
        }
        entry = dir.openNextFile();
    }
    dir.close();
}

bool SDStore::formatForRatputer() {
    if (!_ready) return false;

    Serial.println("[SD] Creating Ratputer directory structure...");
    bool ok = true;
    ok &= ensureDir("/ratputer");
    ok &= ensureDir("/ratputer/config");
    ok &= ensureDir("/ratputer/messages");
    ok &= ensureDir("/ratputer/contacts");
    ok &= ensureDir("/ratputer/identity");

    if (ok) {
        Serial.println("[SD] Directory structure ready");
    } else {
        Serial.println("[SD] Failed to create directories");
    }
    return ok;
}
