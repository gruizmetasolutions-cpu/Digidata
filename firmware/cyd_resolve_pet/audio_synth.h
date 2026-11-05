#pragma once
#include <Arduino.h>

// ============================================================================
// Dataholics Sovereign Audio Engine (ESP32-2432S028 CYD Speaker on GPIO 26)
// Direct Hardware LEDC PWM Synthesizer for ESP32 Arduino Core 2.x & 3.x
// Ensures zero-latency clicks, boot fanfares, and tactical mech lock-on sounds
// ============================================================================

#define AUDIO_PIN 26
#define AUDIO_PIN_ALT 22 // Secondary P3/CN1 GPIO on some CYD revisions

class ScrapHoundAudio {
private:
  bool _muted;
  bool _initialized;

  void _hwTone(int freq, int durationMs) {
    if (_muted || freq <= 0 || durationMs <= 0) return;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcWriteTone(AUDIO_PIN, freq);
    delay(durationMs);
    ledcWrite(AUDIO_PIN, 0);
#else
    tone(AUDIO_PIN, freq, durationMs);
    delay(durationMs);
    noTone(AUDIO_PIN);
#endif
  }

public:
  ScrapHoundAudio() : _muted(false), _initialized(false) {}

  void begin() {
    pinMode(AUDIO_PIN, OUTPUT);
#if ESP_ARDUINO_VERSION_MAJOR >= 3
    ledcAttach(AUDIO_PIN, 2000, 8);
    ledcWrite(AUDIO_PIN, 0);
#else
    noTone(AUDIO_PIN);
#endif
    _initialized = true;
  }

  void setMuted(bool mute) {
    _muted = mute;
    if (_muted) {
#if ESP_ARDUINO_VERSION_MAJOR >= 3
      ledcWrite(AUDIO_PIN, 0);
#else
      noTone(AUDIO_PIN);
#endif
    }
  }

  bool isMuted() const {
    return _muted;
  }

  void playTone(int freq, int durationMs) {
    if (!_initialized) begin();
    _hwTone(freq, durationMs);
  }

  // Crisp 2-stage Tactical Mech UI Click (audible on every screen tap)
  void playTap() {
    if (_muted) return;
    if (!_initialized) begin();
    _hwTone(1850, 18);
    delay(6);
    _hwTone(2950, 24);
  }

  // Aggressive Target-Lock / Mech Snarl when screen is touched
  void playGrowl() {
    if (_muted) return;
    if (!_initialized) begin();
    _hwTone(2400, 20);
    delay(8);
    _hwTone(3200, 25);
    delay(12);
    _hwTone(280, 45);
    delay(10);
    _hwTone(190, 55);
  }

  // Ticket Opened: Tactical Combat Siren
  void playWarKlaxon() {
    if (_muted) return;
    if (!_initialized) begin();
    for (int rep = 0; rep < 2; rep++) {
      for (int f = 600; f <= 1400; f += 100) {
        _hwTone(f, 14);
      }
      delay(25);
      for (int f = 1400; f >= 600; f -= 100) {
        _hwTone(f, 14);
      }
      delay(30);
    }
  }

  // Ticket Updated / Cocky Mode: Sovereign Mecha Radar + Smirk Chime
  void playManiacalLaugh() {
    if (_muted) return;
    if (!_initialized) begin();
    int notes[] = {1174, 1479, 1760, 2349};
    for (int i = 0; i < 4; i++) {
      _hwTone(notes[i], 38);
      delay(15);
    }
  }

  // Ticket Resolved: Sovereign Overdrive Fanfare
  void playEvilChompFanfare() {
    if (_muted) return;
    if (!_initialized) begin();
    int victory[] = {587, 880, 1174, 1479, 1760, 2349};
    for (int i = 0; i < 6; i++) {
      _hwTone(victory[i], 45);
      delay(18);
    }
  }
};
