#include "AudioNotify.h"

void AudioNotify::begin() {
    M5.Speaker.begin();
    M5.Speaker.setVolume(_volume * 255 / 100);
}

void AudioNotify::tone(uint16_t freq, uint16_t durationMs) {
    if (!_enabled) return;
    M5.Speaker.setVolume(_volume * 255 / 100);
    M5.Speaker.tone(freq, durationMs);
    delay(durationMs);
    M5.Speaker.stop();
}

void AudioNotify::playMessage() {
    if (!_enabled) return;
    tone(1000, 50);
    delay(50);
    tone(1000, 50);
}

void AudioNotify::playAnnounce() {
    if (!_enabled) return;
    tone(800, 20);
}

void AudioNotify::playError() {
    if (!_enabled) return;
    for (int i = 0; i < 3; i++) {
        tone(400, 100);
        if (i < 2) delay(50);
    }
}

void AudioNotify::playBoot() {
    if (!_enabled) return;
    tone(440, 60);   // A4
    delay(30);
    tone(554, 60);   // C#5
    delay(30);
    tone(659, 60);   // E5
    delay(30);
    tone(880, 90);   // A5
}
