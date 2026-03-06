#include "FlashStore.h"

bool FlashStore::begin() {
    if (!LittleFS.begin(true)) {  // true = format if mount fails
        Serial.println("[FLASH] LittleFS mount failed!");
        return false;
    }
    _ready = true;

    // Ensure required directories
    ensureDir("/identity");
    ensureDir("/transport");
    ensureDir("/config");
    ensureDir("/contacts");
    ensureDir("/messages");

    Serial.printf("[FLASH] LittleFS ready, total=%lu, used=%lu\n",
                  (unsigned long)LittleFS.totalBytes(),
                  (unsigned long)LittleFS.usedBytes());
    return true;
}

void FlashStore::end() {
    LittleFS.end();
    _ready = false;
}

bool FlashStore::ensureDir(const char* path) {
    if (!_ready) return false;
    if (LittleFS.exists(path)) return true;
    return LittleFS.mkdir(path);
}

bool FlashStore::exists(const char* path) {
    if (!_ready) return false;
    return LittleFS.exists(path);
}

bool FlashStore::remove(const char* path) {
    if (!_ready) return false;
    return LittleFS.remove(path);
}

bool FlashStore::writeAtomic(const char* path, const uint8_t* data, size_t len) {
    if (!_ready) return false;

    // Step 1: Write to .tmp
    String tmpPath = String(path) + ".tmp";
    String bakPath = String(path) + ".bak";

    File f = LittleFS.open(tmpPath.c_str(), "w");
    if (!f) return false;
    size_t written = f.write(data, len);
    f.close();
    if (written != len) {
        LittleFS.remove(tmpPath.c_str());
        return false;
    }

    // Step 2: Verify .tmp by reading back
    File verify = LittleFS.open(tmpPath.c_str(), "r");
    if (!verify || verify.size() != len) {
        if (verify) verify.close();
        LittleFS.remove(tmpPath.c_str());
        return false;
    }
    verify.close();

    // Step 3: Rename current to .bak (if exists)
    if (LittleFS.exists(path)) {
        LittleFS.remove(bakPath.c_str());
        LittleFS.rename(path, bakPath.c_str());
    }

    // Step 4: Rename .tmp to primary
    if (!LittleFS.rename(tmpPath.c_str(), path)) {
        // Restore from backup on failure
        if (LittleFS.exists(bakPath.c_str())) {
            LittleFS.rename(bakPath.c_str(), path);
        }
        return false;
    }

    return true;
}

bool FlashStore::readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& bytesRead) {
    if (!_ready) return false;

    // Try primary first
    File f = LittleFS.open(path, "r");
    if (!f) {
        // Try backup
        String bakPath = String(path) + ".bak";
        f = LittleFS.open(bakPath.c_str(), "r");
        if (!f) return false;
        Serial.printf("[FLASH] Restored from backup: %s\n", path);
    }

    bytesRead = f.readBytes((char*)buffer, maxLen);
    f.close();
    return bytesRead > 0;
}

bool FlashStore::writeString(const char* path, const String& data) {
    return writeAtomic(path, (const uint8_t*)data.c_str(), data.length());
}

bool FlashStore::writeDirect(const char* path, const uint8_t* data, size_t len) {
    if (!_ready) return false;
    File f = LittleFS.open(path, "w");
    if (!f) return false;
    size_t written = f.write(data, len);
    f.flush();
    f.close();
    return written == len;
}

String FlashStore::readString(const char* path) {
    if (!_ready) return "";
    File f = LittleFS.open(path, "r");
    if (!f) {
        String bakPath = String(path) + ".bak";
        f = LittleFS.open(bakPath.c_str(), "r");
        if (!f) return "";
    }
    String result = f.readString();
    f.close();
    return result;
}

bool FlashStore::format() {
    Serial.println("[FLASH] Formatting LittleFS...");
    LittleFS.end();
    bool ok = LittleFS.format();
    if (ok) {
        ok = begin();
    }
    return ok;
}
