#pragma once

#include <Arduino.h>
#include <M5Unified.h>

class AudioNotify {
public:
    void begin();

    // Notification sounds
    void playMessage();     // Double-beep: 50ms 1kHz × 2
    void playAnnounce();    // Single tick: 20ms 800Hz
    void playError();       // Triple-beep: 100ms 400Hz × 3
    void playBoot();        // Ascending chime: A4→C#5→E5→A5

    // Settings
    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }
    void setVolume(uint8_t vol) { _volume = vol; }
    uint8_t volume() const { return _volume; }

private:
    void tone(uint16_t freq, uint16_t durationMs);

    bool _enabled = true;
    uint8_t _volume = 80;  // 0-100
};
