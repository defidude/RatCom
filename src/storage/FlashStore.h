#pragma once

#include <Arduino.h>
#include <LittleFS.h>

class FlashStore {
public:
    bool begin();
    void end();

    // Atomic write: .tmp → validate → rename .bak → rename .tmp → primary
    bool writeAtomic(const char* path, const uint8_t* data, size_t len);

    // Simple direct write (no rename dance — for small config files)
    bool writeDirect(const char* path, const uint8_t* data, size_t len);
    bool readFile(const char* path, uint8_t* buffer, size_t maxLen, size_t& bytesRead);

    // String convenience wrappers
    bool writeString(const char* path, const String& data);
    String readString(const char* path);

    // Directory operations
    bool ensureDir(const char* path);
    bool exists(const char* path);
    bool remove(const char* path);

    // Format (factory reset)
    bool format();

    bool isReady() const { return _ready; }

private:
    bool _ready = false;
};
