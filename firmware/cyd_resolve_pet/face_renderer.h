#pragma once

#include <Arduino.h>
#include <time.h>
#include "cyd_display.h"
#include "pet_renderer.h" // Reuses PetMood and TicketItem types for drop-in compatibility

// ============================================================================
// Dataholics DigiVice — Smartwatch OS & 2.5D Expressive Cyber-Face Engine
// Target: ESP32-2432S028 (CYD 320x240 16-bit RGB565)
// 100% Flicker-Free Architecture:
//   - _canvas (220x140) double-buffers the central Eyes / Smartwatch Dial
//   - _banner (320x36) double-buffers BOTH the Top Bar (y=0) & Bottom Dock (y=204)
//   - Static screens & side bezels use dirty-flag differential rendering
// ============================================================================

class CyberFaceRenderer {
private:
  LGFX *_gfx;
  LGFX_Sprite _canvas;
  LGFX_Sprite _banner;
  bool _spriteReady;
  bool _bannerReady;
  bool _bgCleared;

  // Eye & Brow Physics State
  uint32_t _animTick;
  float _gazeX, _gazeY;
  float _targetGazeX, _targetGazeY;
  float _browX, _browY;
  float _blink;
  uint32_t _nextBlinkTick;
  float _upperLid;
  float _lowerLid;
  float _browAngle;
  float _browLift;
  float _pupilScale;

  int _screenShakeX;
  int _screenShakeY;

  uint32_t _idleTimer;
  IdleSubState _idleSubState;

  bool _touchActive;

  // Color Palette (RGB565)
  static const uint16_t C_VOID_BG     = 0x0000; // Pure OLED Black for Watch contrast
  static const uint16_t C_WATCH_CARD  = 0x0862; // Deep Slate Watch Card (#080D14)
  static const uint16_t C_WATCH_RING  = 0x1926; // Subtle Watch Bezel Ring
  static const uint16_t C_NEON_CYAN   = 0x07FF; // Electric Cyan (#00FFFF)
  static const uint16_t C_CYAN_DIM    = 0x03EF; // Deep Teal Halo
  static const uint16_t C_DATA_ORANGE = 0xFB80; // Dataholics Signature Orange (#FF7300)
  static const uint16_t C_WHITE_GLOW  = 0xFFFF; // Specular Highlight
  static const uint16_t C_GLASS_SHINE = 0xBDF7; // Secondary Reflection
  static const uint16_t C_ALERT_RED   = 0xF986; // Crimson Alert
  static const uint16_t C_AMBER_GOLD  = 0xFD20; // Happy / Laugh Gold
  static const uint16_t C_LIME_GREEN  = 0x2FE8; // Resolved / Emerald
  static const uint16_t C_PLASMA_PURP = 0xC99F; // Poked / Surprised Purple
  static const uint16_t C_TEXT_MUTED  = 0x8430; // Muted Slate Label

  // Central Sprite Geometry (fits safely in internal ESP32 DRAM without PSRAM)
  static const int FACE_W = 220;
  static const int FACE_H = 140;
  static const int FACE_X = 50;  // (320 - 220) / 2
  static const int FACE_Y = 48;  // Between Top Bar (0..36) and Bottom Dock (204..240)

public:
  CyberFaceRenderer(LGFX *gfx)
      : _gfx(gfx), _canvas(gfx), _banner(gfx),
        _spriteReady(false), _bannerReady(false), _bgCleared(false),
        _animTick(0), _gazeX(0), _gazeY(0), _targetGazeX(0), _targetGazeY(0),
        _browX(0), _browY(0), _blink(0), _nextBlinkTick(60),
        _upperLid(0.05f), _lowerLid(0.0f), _browAngle(0.0f), _browLift(0.0f),
        _pupilScale(1.0f), _screenShakeX(0), _screenShakeY(0),
        _idleTimer(0), _idleSubState(IDLE_REST), _touchActive(false) {}

  void begin() {
    // 1. Allocate 220x140 central sprite (61.6 KB at 16-bit, or 30.8 KB at 8-bit fallback)
    _canvas.setColorDepth(16);
    if (_canvas.createSprite(FACE_W, FACE_H) != nullptr) {
      _spriteReady = true;
    } else {
      _canvas.setColorDepth(8);
      if (_canvas.createSprite(FACE_W, FACE_H) != nullptr) {
        _spriteReady = true;
      }
    }

    // 2. Allocate 320x36 universal horizontal bar sprite (23 KB) for flicker-free Top & Bottom bars
    _banner.setColorDepth(16);
    if (_banner.createSprite(320, 36) != nullptr) {
      _bannerReady = true;
    } else {
      _banner.setColorDepth(8);
      if (_banner.createSprite(320, 36) != nullptr) {
        _bannerReady = true;
      }
    }
  }

  void setTouch(bool active, int16_t tx = 160, int16_t ty = 120) {
    _touchActive = active;
    if (active) {
      _targetGazeX = constrain((tx - 160.0f) / 110.0f, -1.0f, 1.0f);
      _targetGazeY = constrain((ty - 118.0f) / 70.0f, -1.0f, 1.0f);
      _idleTimer = 0;
      _idleSubState = IDLE_REST;
    }
  }

  void setShake(int intensity) {
    if (intensity > 0) {
      _screenShakeX = random(-intensity, intensity + 1);
      _screenShakeY = random(-intensity, intensity + 1);
    } else {
      _screenShakeX = 0;
      _screenShakeY = 0;
    }
  }

  bool isSleeping() const { return (_idleSubState == IDLE_SLEEP); }
  bool isDozing() const { return (_idleSubState == IDLE_DOZE); }

  void wakeUp() {
    _idleSubState = IDLE_REST;
    _idleTimer = 0;
    _blink = 0;
    _upperLid = 0;
  }

  void clearScreen() {
    _gfx->fillScreen(C_VOID_BG);
    _bgCleared = false;
  }

  // Helper: Get formatted time HH:MM, :SS, and Date string (uses NTP if synced, or uptime clock)
  void getFormattedTime(String &outHM, String &outSec, String &outDate) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
      // Adjust UTC-6 (Mexico City Standard Time)
      time_t local = now - (6 * 3600);
      struct tm *t = gmtime(&local);
      char bufHM[8], bufSec[6], bufDate[16];
      snprintf(bufHM, sizeof(bufHM), "%02d:%02d", t->tm_hour, t->tm_min);
      snprintf(bufSec, sizeof(bufSec), ":%02d", t->tm_sec);
      const char *days[] = {"DOM", "LUN", "MAR", "MIE", "JUE", "VIE", "SAB"};
      const char *months[] = {"ENE", "FEB", "MAR", "ABR", "MAY", "JUN", "JUL", "AGO", "SEP", "OCT", "NOV", "DIC"};
      snprintf(bufDate, sizeof(bufDate), "%s %02d %s", days[t->tm_wday % 7], t->tm_mday, months[t->tm_mon % 12]);
      outHM = String(bufHM);
      outSec = String(bufSec);
      outDate = String(bufDate);
    } else {
      // Graceful Smartwatch clock before NTP sync (starts at 10:45 + uptime)
      unsigned long totalSec = (millis() / 1000) + (10 * 3600) + (45 * 60);
      int hh = (totalSec / 3600) % 24;
      int mm = (totalSec / 60) % 60;
      int ss = totalSec % 60;
      char bufHM[8], bufSec[6];
      snprintf(bufHM, sizeof(bufHM), "%02d:%02d", hh, mm);
      snprintf(bufSec, sizeof(bufSec), ":%02d", ss);
      outHM = String(bufHM);
      outSec = String(bufSec);
      outDate = "DATAHOLICS OS";
    }
  }

  // ==========================================================================
  // EXECUTIVE DATAHOLICS BOOT ANIMATION (~2.5 Seconds, Zero Flicker)
  // ==========================================================================
  void playBootAnimation(void (*audioCb)(int, int)) {
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    // Subtle Watch OS Framing
    _gfx->drawRoundRect(6, 6, 308, 228, 18, C_WATCH_RING);
    _gfx->drawRoundRect(8, 8, 304, 224, 16, 0x10A2);

    // Draw Iconic Dataholics Split-Wing Orange Emblem at center (cx=160, cy=72)
    int cx = 160;
    int cy = 70;
    // Left Wing
    _gfx->fillTriangle(cx - 46, cy - 22, cx - 6, cy - 30, cx - 6, cy + 30, C_DATA_ORANGE);
    _gfx->fillTriangle(cx - 46, cy - 22, cx - 6, cy + 30, cx - 46, cy + 22, C_DATA_ORANGE);
    // Right Wing
    _gfx->fillTriangle(cx + 6, cy - 30, cx + 46, cy - 22, cx + 46, cy + 22, C_DATA_ORANGE);
    _gfx->fillTriangle(cx + 6, cy - 30, cx + 46, cy + 22, cx + 6, cy + 30, C_DATA_ORANGE);
    // Inner Wing Hollows
    _gfx->fillCircle(cx - 25, cy, 12, C_VOID_BG);
    _gfx->fillCircle(cx + 25, cy, 12, C_VOID_BG);
    _gfx->fillRect(cx - 25, cy - 5, 50, 10, C_VOID_BG);

    // Brand Typography
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->setTextSize(2);
    _gfx->drawString("D A T A H O L I C S", 46, 114);

    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString("SOVEREIGN SMARTWATCH OS // v3.5", 64, 138);

    // Executive Clearance Pill
    _gfx->fillRoundRect(54, 154, 212, 22, 6, C_WATCH_CARD);
    _gfx->drawRoundRect(54, 154, 212, 22, 6, C_AMBER_GOLD);
    _gfx->setTextColor(C_AMBER_GOLD);
    _gfx->drawString("ALONSO RUIZ  •  CEO EDITION", 78, 161);

    // Smooth Progress Track (drawn once, filled incrementally -> ZERO flicker!)
    _gfx->drawRoundRect(40, 190, 240, 12, 5, C_WATCH_RING);
    _gfx->endWrite();

    // 2.5-Second Progressive Fill (25 steps x 85ms = ~2.1s + chime = ~2.5s total)
    for (int step = 1; step <= 25; step++) {
      int w = (step * 234) / 25;
      _gfx->fillRoundRect(43, 193, w, 6, 3, C_DATA_ORANGE);
      if (audioCb) {
        if (step == 1) audioCb(523, 35);       // C5
        else if (step == 8) audioCb(659, 35);  // E5
        else if (step == 16) audioCb(784, 35); // G5
        else if (step == 25) audioCb(1046, 80);// C6
      }
      delay(80);
    }

    delay(180);
    _gfx->fillScreen(C_VOID_BG);
    _bgCleared = false;
  }

  // ==========================================================================
  // CARATULA 1: FULL-SCREEN EXPRESSIVE CYBER-FACE (Zero-Flicker Watch Bezel)
  // ==========================================================================
  void renderFrame(PetMood mood, const String &ticketTitle, int ticketId, const String &ticketStatus, int activeCount, int resolvedCount, const String &deviceMac) {
    _animTick++;
    _idleTimer++;

    uint16_t accent = C_NEON_CYAN;
    if (mood == MOOD_ALARM) accent = C_ALERT_RED;
    else if (mood == MOOD_LAUGH) accent = C_AMBER_GOLD;
    else if (mood == MOOD_CHOMP) accent = C_LIME_GREEN;
    else if (mood == MOOD_POKED) accent = C_PLASMA_PURP;

    // Draw static Smartwatch outer bezel arches ONCE when entering screen (never flickers!)
    if (!_bgCleared) {
      _gfx->startWrite();
      _gfx->fillScreen(C_VOID_BG);
      // Curved Smartwatch Side Bezel Guides (outside the 220x140 center sprite)
      _gfx->drawRoundRect(16, 40, 288, 156, 22, C_WATCH_RING);
      _gfx->drawFastVLine(28, 72, 92, 0x10A2);
      _gfx->drawFastVLine(292, 72, 92, 0x10A2);
      // Subtle side ticks
      for (int ty = 80; ty <= 152; ty += 18) {
        _gfx->drawFastHLine(22, ty, 5, C_WATCH_RING);
        _gfx->drawFastHLine(293, ty, 5, C_WATCH_RING);
      }
      _gfx->endWrite();
      _bgCleared = true;
    }

    // Autonomous Saccades & Blinking
    if (!_touchActive) {
      if (_animTick % 65 == 0) {
        _targetGazeX = (random(-10, 11) / 10.0f) * 0.90f;
        _targetGazeY = (random(-10, 11) / 10.0f) * 0.65f;
      } else if (_animTick % 170 == 0) {
        _targetGazeX = 0;
        _targetGazeY = 0;
      }
    }

    if (_animTick >= _nextBlinkTick) {
      _blink = 1.0f;
      _nextBlinkTick = _animTick + 85 + random(0, 110);
    }
    _blink += (0.0f - _blink) * 0.32f;

    _gazeX += (_targetGazeX - _gazeX) * 0.24f;
    _gazeY += (_targetGazeY - _gazeY) * 0.24f;
    _browX += (_gazeX - _browX) * 0.14f;
    _browY += (_gazeY - _browY) * 0.14f;

    // Mood target variables
    float tUpper = 0.04f;
    float tLower = 0.0f;
    float tAng = 0.0f;
    float tLift = 0.0f;
    float tPupil = 1.0f;

    if (mood == MOOD_ALARM) {
      tUpper = 0.28f;
      tAng = 0.42f;
      tLift = -5.0f;
      tPupil = 0.70f;
    } else if (mood == MOOD_LAUGH) {
      tLower = 0.52f;
      tAng = -0.25f;
      tLift = 6.0f + sin(_animTick * 0.4f) * 2.5f;
    } else if (mood == MOOD_CHOMP) {
      tLower = 0.35f;
      tLift = 8.0f;
      tPupil = 1.25f;
    } else if (mood == MOOD_POKED) {
      tPupil = 0.60f;
      tLift = 9.0f;
    }

    _upperLid += (max(tUpper, _blink) - _upperLid) * 0.28f;
    _lowerLid += (tLower - _lowerLid) * 0.24f;
    _browAngle += (tAng - _browAngle) * 0.22f;
    _browLift += (tLift - _browLift) * 0.22f;
    _pupilScale += (tPupil - _pupilScale) * 0.22f;

    String strHM, strSec, strDate;
    getFormattedTime(strHM, strSec, strDate);

    // ------------------------------------------------------------------------
    // 1. TOP SMARTWATCH BAR (y = 0..35) -> Rendered in _banner Sprite (0% Flicker!)
    // ------------------------------------------------------------------------
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);

      // Sleek floating top capsule
      _banner.fillRoundRect(6, 3, 308, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(6, 3, 308, 28, 12, C_WATCH_RING);

      // Left Pill: Switch to Caratula 2 (Smartwatch Mode)
      _banner.fillRoundRect(10, 6, 86, 22, 9, 0x10A2);
      _banner.drawRoundRect(10, 6, 86, 22, 9, C_DATA_ORANGE);
      _banner.setTextColor(C_DATA_ORANGE);
      _banner.setTextSize(1);
      _banner.drawString("RELOJ OS >", 20, 13);

      // Center: Live Watch Time HH:MM:SS + Pulse Dot
      bool beat = ((_animTick / 12) % 2 == 0);
      _banner.fillCircle(114, 17, beat ? 3 : 2, accent);
      _banner.setTextColor(C_WHITE_GLOW);
      _banner.setTextSize(2);
      _banner.drawString(strHM, 124, 9);
      _banner.setTextColor(accent);
      _banner.setTextSize(1);
      _banner.drawString(strSec, 186, 14);

      // Right Pill: Active Tickets Complication
      uint16_t actCol = (activeCount > 0) ? C_ALERT_RED : C_LIME_GREEN;
      _banner.fillRoundRect(222, 6, 88, 22, 9, 0x10A2);
      _banner.drawRoundRect(222, 6, 88, 22, 9, actCol);
      _banner.fillCircle(233, 17, 3, actCol);
      _banner.setTextColor(C_WHITE_GLOW);
      _banner.drawString(String("ACT:") + activeCount + " OK:" + resolvedCount, 241, 13);

      _banner.pushSprite(_gfx, 0, 0);
    }

    // ------------------------------------------------------------------------
    // 2. CENTER EXPRESSIVE EYES (220x140) -> Rendered in _canvas Sprite (0% Flicker!)
    // ------------------------------------------------------------------------
    if (_spriteReady) {
      _canvas.fillScreen(C_VOID_BG);

      // Subtle curved inner watch grid
      for (int gy = 18; gy < FACE_H; gy += 28) {
        _canvas.drawFastHLine(12, gy, FACE_W - 24, 0x0841);
      }

      // Draw Left Eye (cx=58, cy=62) and Right Eye (cx=162, cy=62) — larger & deeper!
      _drawSingleEye(58, 62, 74, 56, true, accent, 1.0f);
      _drawSingleEye(162, 62, 74, 56, false, accent, 1.0f);

      // Draw Expressive Vocal Wave / Mouth at bottom of visor (y=118)
      int mx = (FACE_W / 2) + (int)(_gazeX * 9.0f);
      int my = 118 + (int)(_gazeY * 4.0f);

      if (mood == MOOD_LAUGH || mood == MOOD_CHOMP) {
        _canvas.fillCircle(mx, my - 4, 15, accent);
        _canvas.fillCircle(mx, my - 7, 14, C_VOID_BG);
        _canvas.fillRect(mx - 18, my - 22, 36, 15, C_VOID_BG);
      } else if (mood == MOOD_ALARM) {
        int prevX = mx - 26;
        int prevY = my;
        for (int i = -22; i <= 26; i += 4) {
          int wy = my + (int)(sin((i * 0.7f) + (_animTick * 0.6f)) * 6.0f);
          _canvas.drawLine(prevX, prevY, mx + i, wy, accent);
          prevX = mx + i;
          prevY = wy;
        }
      } else {
        int prevX = mx - 22;
        int prevY = my;
        for (int i = -20; i <= 22; i += 2) {
          float env = 1.0f - (abs(i) / 24.0f);
          int wy = my + (int)(sin((i * 0.4f) + (_animTick * 0.25f)) * 4.0f * env);
          _canvas.drawLine(prevX, prevY, mx + i, wy, accent);
          prevX = mx + i;
          prevY = wy;
        }
      }

      _canvas.pushSprite(_gfx, FACE_X + _screenShakeX, FACE_Y + _screenShakeY);
    }

    // ------------------------------------------------------------------------
    // 3. BOTTOM SMARTWATCH DOCK (y = 204..239) -> Rendered in _banner Sprite (0% Flicker!)
    // ------------------------------------------------------------------------
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);

      // Left Pill: CEO Badge
      _banner.fillRoundRect(6, 4, 96, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(6, 4, 96, 28, 12, accent);
      _banner.setTextColor(C_WHITE_GLOW);
      _banner.setTextSize(1);
      _banner.drawString("ALONSO CEO", 20, 14);

      // Center Pill: Mood / Expression Selector
      _banner.fillRoundRect(108, 4, 104, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(108, 4, 104, 28, 12, C_DATA_ORANGE);
      _banner.setTextColor(C_DATA_ORANGE);
      const char *moodLabel = "MODO: NORMAL";
      if (mood == MOOD_ALARM) moodLabel = "MODO: ALERTA";
      else if (mood == MOOD_LAUGH) moodLabel = "MODO: FELIZ";
      else if (mood == MOOD_CHOMP) moodLabel = "MODO: EUFORIA";
      else if (mood == MOOD_POKED) moodLabel = "MODO: SORPRESA";
      _banner.drawString(moodLabel, 120, 14);

      // Right Pill: Incident Stream
      _banner.fillRoundRect(218, 4, 96, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(218, 4, 96, 28, 12, C_NEON_CYAN);
      _banner.setTextColor(C_NEON_CYAN);
      _banner.drawString("TICKETS >", 238, 14);

      _banner.pushSprite(_gfx, 0, 204);
    }
  }

  // ==========================================================================
  // CARATULA 2: SMARTWATCH EXECUTIVE CHRONOGRAPH & COMPLICATIONS (Zero Flicker!)
  // ==========================================================================
  void drawSmartwatchScreen(PetMood mood, const String &ticketTitle, int ticketId, int activeCount, int resolvedCount, bool wifiOnline, int rssi) {
    _animTick++;

    uint16_t accent = C_NEON_CYAN;
    if (mood == MOOD_ALARM) accent = C_ALERT_RED;
    else if (mood == MOOD_LAUGH) accent = C_AMBER_GOLD;
    else if (mood == MOOD_CHOMP) accent = C_LIME_GREEN;

    // Update subtle eye blink for the mini guardian eyes complication
    if (_animTick >= _nextBlinkTick) {
      _blink = 1.0f;
      _nextBlinkTick = _animTick + 80 + random(0, 100);
    }
    _blink += (0.0f - _blink) * 0.30f;
    _upperLid += (_blink - _upperLid) * 0.30f;

    // Draw side watch complication rails ONCE when entering Smartwatch mode
    if (!_bgCleared) {
      _gfx->startWrite();
      _gfx->fillScreen(C_VOID_BG);

      // Left Side Rail: System Pulse Gauge
      _gfx->fillRoundRect(6, 46, 38, 144, 12, C_WATCH_CARD);
      _gfx->drawRoundRect(6, 46, 38, 144, 12, C_WATCH_RING);
      _gfx->setTextColor(C_TEXT_MUTED);
      _gfx->setTextSize(1);
      _gfx->drawString("PULSO", 10, 56);
      _gfx->drawRoundRect(19, 72, 12, 92, 5, 0x18E3);
      _gfx->fillRoundRect(21, 96, 8, 66, 3, C_NEON_CYAN);
      _gfx->setTextColor(C_NEON_CYAN);
      _gfx->drawString("98%", 14, 172);

      // Right Side Rail: Network / Uplink Complication
      _gfx->fillRoundRect(276, 46, 38, 144, 12, C_WATCH_CARD);
      _gfx->drawRoundRect(276, 46, 38, 144, 12, C_WATCH_RING);
      _gfx->setTextColor(C_TEXT_MUTED);
      _gfx->drawString("RED", 286, 56);
      uint16_t netCol = wifiOnline ? C_LIME_GREEN : C_DATA_ORANGE;
      // Signal bars
      _gfx->fillRect(283, 134, 4, 10, netCol);
      _gfx->fillRect(290, 124, 4, 20, netCol);
      _gfx->fillRect(297, 114, 4, 30, netCol);
      _gfx->fillRect(304, 102, 4, 42, wifiOnline ? netCol : 0x18E3);
      _gfx->setTextColor(netCol);
      _gfx->drawString(wifiOnline ? "SYNC" : "LOC", 283, 172);

      _gfx->endWrite();
      _bgCleared = true;
    }

    String strHM, strSec, strDate;
    getFormattedTime(strHM, strSec, strDate);

    // ------------------------------------------------------------------------
    // 1. TOP SMARTWATCH CROWN BAR (y = 0..35) -> Rendered in _banner
    // ------------------------------------------------------------------------
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);
      _banner.fillRoundRect(6, 3, 308, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(6, 3, 308, 28, 12, C_WATCH_RING);

      // Left Pill: Tap to switch to Full-Screen Face
      _banner.fillRoundRect(10, 6, 92, 22, 9, 0x10A2);
      _banner.drawRoundRect(10, 6, 92, 22, 9, C_NEON_CYAN);
      _banner.setTextColor(C_NEON_CYAN);
      _banner.setTextSize(1);
      _banner.drawString("< ROSTRO 2.5D", 16, 13);

      // Center: Date / OS Title
      _banner.setTextColor(C_WHITE_GLOW);
      _banner.drawString(strDate, 124, 13);

      // Right: Wi-Fi Live Pill
      uint16_t wfCol = wifiOnline ? C_LIME_GREEN : C_DATA_ORANGE;
      _banner.fillCircle(238, 17, 4, wfCol);
      _banner.setTextColor(wfCol);
      _banner.drawString(wifiOnline ? "WIFI ONLINE" : "MODO LOCAL", 248, 13);

      _banner.pushSprite(_gfx, 0, 0);
    }

    // ------------------------------------------------------------------------
    // 2. CENTER SMARTWATCH CHRONOGRAPH DIAL (220x140) -> Rendered in _canvas
    // ------------------------------------------------------------------------
    if (_spriteReady) {
      _canvas.fillScreen(C_VOID_BG);

      // Outer Watch Dial Card
      _canvas.fillRoundRect(0, 0, FACE_W, FACE_H, 16, C_WATCH_CARD);
      _canvas.drawRoundRect(0, 0, FACE_W, FACE_H, 16, C_WATCH_RING);

      // Top Half: Giant Smartwatch Digital Clock + Mini Guardian Eyes
      _canvas.setTextColor(C_WHITE_GLOW);
      _canvas.setTextSize(4);
      _canvas.drawString(strHM, 14, 12);

      // Seconds badge + Orange accent
      _canvas.fillRoundRect(138, 12, 42, 20, 6, 0x10A2);
      _canvas.drawRoundRect(138, 12, 42, 20, 6, C_DATA_ORANGE);
      _canvas.setTextColor(C_DATA_ORANGE);
      _canvas.setTextSize(1);
      _canvas.drawString(strSec, 148, 18);

      // Mini Animated Guardian Eyes inside top-right of Watch Dial!
      int eyeH = max(2, (int)(12.0f * (1.0f - _upperLid)));
      int eyeY = 38 + (12 - eyeH) / 2;
      _canvas.fillRoundRect(184, eyeY, 12, eyeH, 3, accent);
      _canvas.fillRoundRect(200, eyeY, 12, eyeH, 3, accent);

      // Live ECG Heartbeat Wave under clock (y = 52)
      _canvas.drawFastHLine(14, 54, 192, 0x18E3);
      int ecgHead = (_animTick * 4) % 188;
      for (int x = 0; x < 188; x += 2) {
        int dist = abs(x - ecgHead);
        int ey = 54;
        if (dist < 14) {
          ey = 54 + (int)(sin((x - ecgHead) * 0.6f) * 7.0f);
        }
        _canvas.drawPixel(16 + x, ey, C_DATA_ORANGE);
        _canvas.drawPixel(16 + x, ey + 1, C_DATA_ORANGE);
      }

      // Bottom Half: Two Smartwatch Complication Cards (Activos & Resueltos) + Ticker
      // Complication 1: Tickets Activos (Left Ring Card)
      _canvas.fillRoundRect(10, 66, 96, 46, 10, 0x10A2);
      uint16_t actCol = (activeCount > 0) ? C_ALERT_RED : C_NEON_CYAN;
      _canvas.drawRoundRect(10, 66, 96, 46, 10, actCol);
      // Circular Ring Complication
      _canvas.drawCircle(30, 89, 13, C_WATCH_RING);
      _canvas.drawCircle(30, 89, 12, actCol);
      _canvas.fillCircle(30, 89, 4, actCol);
      _canvas.setTextColor(C_TEXT_MUTED);
      _canvas.setTextSize(1);
      _canvas.drawString("ACTIVOS", 50, 74);
      _canvas.setTextColor(C_WHITE_GLOW);
      _canvas.setTextSize(2);
      _canvas.drawString(String(activeCount), 50, 88);

      // Complication 2: Resueltos Hoy (Right Ring Card)
      _canvas.fillRoundRect(114, 66, 96, 46, 10, 0x10A2);
      _canvas.drawRoundRect(114, 66, 96, 46, 10, C_LIME_GREEN);
      _canvas.drawCircle(134, 89, 13, C_WATCH_RING);
      _canvas.drawCircle(134, 89, 12, C_LIME_GREEN);
      _canvas.fillCircle(134, 89, 4, C_LIME_GREEN);
      _canvas.setTextColor(C_TEXT_MUTED);
      _canvas.setTextSize(1);
      _canvas.drawString("RESUELTOS", 152, 74);
      _canvas.setTextColor(C_LIME_GREEN);
      _canvas.setTextSize(2);
      _canvas.drawString(String(resolvedCount), 152, 88);

      // Bottom Sub-Strip inside Watch Dial: Latest Ticket Feed
      _canvas.setTextColor(C_NEON_CYAN);
      _canvas.setTextSize(1);
      String shortTkt = String("#") + ticketId + " " + ticketTitle;
      _canvas.drawString(shortTkt.substring(0, 32), 14, 121);

      _canvas.pushSprite(_gfx, FACE_X, FACE_Y);
    }

    // ------------------------------------------------------------------------
    // 3. BOTTOM SMARTWATCH DOCK (y = 204..239) -> Rendered in _banner
    // ------------------------------------------------------------------------
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);

      // Left Pill: Return to Expressive Eyes
      _banner.fillRoundRect(6, 4, 96, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(6, 4, 96, 28, 12, C_NEON_CYAN);
      _banner.setTextColor(C_NEON_CYAN);
      _banner.setTextSize(1);
      _banner.drawString("ROSTRO 2.5D", 18, 14);

      // Center Pill: Executive Badge
      _banner.fillRoundRect(108, 4, 104, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(108, 4, 104, 28, 12, C_AMBER_GOLD);
      _banner.setTextColor(C_AMBER_GOLD);
      _banner.drawString("GAFETE CEO", 128, 14);

      // Right Pill: ServiceDesk Incidents
      _banner.fillRoundRect(218, 4, 96, 28, 12, C_WATCH_CARD);
      _banner.drawRoundRect(218, 4, 96, 28, 12, C_DATA_ORANGE);
      _banner.setTextColor(C_DATA_ORANGE);
      _banner.drawString("TICKETS >", 238, 14);

      _banner.pushSprite(_gfx, 0, 204);
    }
  }

  // ==========================================================================
  // SECONDARY SCREENS (Zero-Flicker Differential Rendering!)
  // ==========================================================================
  void drawConferenceBadge(const String &nameFirst, const String &nameLast, const String &role, const String &mac) {
    _animTick++;

    // Only draw the static badge card ONCE when entering Badge Mode (0% flicker!)
    if (!_bgCleared) {
      _gfx->startWrite();
      _gfx->fillScreen(C_VOID_BG);

      _gfx->drawRoundRect(6, 6, 308, 228, 16, C_DATA_ORANGE);
      _gfx->drawRoundRect(8, 8, 304, 224, 14, C_WATCH_RING);

      _gfx->fillRoundRect(12, 12, 296, 24, 8, C_WATCH_CARD);
      _gfx->setTextColor(C_AMBER_GOLD);
      _gfx->setTextSize(1);
      _gfx->drawString("DATAHOLICS SOVEREIGN AI // VIP CREDENTIAL", 28, 20);

      _gfx->setTextSize(3);
      _gfx->setTextColor(C_WHITE_GLOW);
      _gfx->drawString(nameFirst, 40, 122);
      _gfx->setTextColor(C_DATA_ORANGE);
      _gfx->drawString(nameLast, 172, 122);

      _gfx->fillRoundRect(32, 162, 256, 26, 8, C_WATCH_CARD);
      _gfx->drawRoundRect(32, 162, 256, 26, 8, C_NEON_CYAN);
      _gfx->setTextColor(C_NEON_CYAN);
      _gfx->setTextSize(1);
      _gfx->drawString("CEO  •  CHIEF EXECUTIVE OFFICER", 60, 171);

      _gfx->setTextColor(C_TEXT_MUTED);
      _gfx->drawString(String("ID: ") + mac + "  •  TOCA PARA VOLVER", 36, 206);
      _gfx->endWrite();
      _bgCleared = true;
    }

    // Animate only the mini double-buffered eyes in the top portion using _banner!
    if (_bannerReady) {
      if (_animTick % 55 == 0) {
        _targetGazeX = (random(-10, 11) / 10.0f) * 0.7f;
      }
      _gazeX += (_targetGazeX - _gazeX) * 0.2f;
      if (_animTick >= _nextBlinkTick) {
        _blink = 1.0f;
        _nextBlinkTick = _animTick + 90 + random(0, 90);
      }
      _blink += (0.0f - _blink) * 0.3f;

      _banner.fillScreen(C_VOID_BG);
      int gx = (int)(_gazeX * 8.0f);
      int eh = max(3, (int)(26.0f * (1.0f - _blink)));
      int ey = 5 + (26 - eh) / 2;

      _banner.fillRoundRect(104 + gx, ey, 42, eh, 9, C_NEON_CYAN);
      _banner.fillRoundRect(174 + gx, ey, 42, eh, 9, C_NEON_CYAN);
      if (eh > 10) {
        _banner.fillCircle(125 + gx * 2, 18, 6, C_VOID_BG);
        _banner.fillCircle(195 + gx * 2, 18, 6, C_VOID_BG);
        _banner.fillCircle(123 + gx * 2, 16, 2, C_WHITE_GLOW);
        _banner.fillCircle(193 + gx * 2, 16, 2, C_WHITE_GLOW);
      }
      _banner.pushSprite(_gfx, 0, 56);
    }
  }

  void drawTicketListScreen(const TicketItem tickets[], int count) {
    if (_bgCleared) return; // Static screen: draw once to eliminate 100% of flicker!
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    _gfx->fillRoundRect(4, 2, 312, 26, 8, C_WATCH_CARD);
    _gfx->drawRoundRect(4, 2, 312, 26, 8, C_NEON_CYAN);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->setTextSize(1);
    _gfx->drawString("SERVICEDESK INCIDENTS", 14, 11);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->drawString("< VOLVER", 248, 11);

    for (int i = 0; i < 4; i++) {
      int y = 32 + i * 42;
      if (i < count) {
        _gfx->fillRoundRect(8, y, 304, 38, 8, C_WATCH_CARD);
        _gfx->drawRoundRect(8, y, 304, 38, 8, C_WATCH_RING);

        uint16_t stColor = (tickets[i].status == "resolved" || tickets[i].status == "RESOLVED") ? C_LIME_GREEN :
                           ((tickets[i].status == "in_progress" || tickets[i].status == "UPDATED") ? C_AMBER_GOLD : C_ALERT_RED);
        _gfx->fillRoundRect(10, y + 4, 5, 30, 2, stColor);

        _gfx->setTextColor(stColor);
        _gfx->drawString(String("#") + tickets[i].id + " [" + tickets[i].status + "] PRIO: " + tickets[i].priority, 22, y + 6);

        _gfx->setTextColor(C_WHITE_GLOW);
        _gfx->drawString(tickets[i].title.substring(0, 42), 22, y + 21);
      } else {
        _gfx->fillRoundRect(8, y, 304, 38, 8, 0x0410);
        _gfx->drawRoundRect(8, y, 304, 38, 8, 0x10A2);
        _gfx->setTextColor(C_TEXT_MUTED);
        _gfx->drawString("-- SIN INCIDENTES EN COLA --", 72, y + 15);
      }
    }

    _gfx->fillRoundRect(4, 204, 312, 32, 10, C_WATCH_CARD);
    _gfx->drawRoundRect(4, 204, 312, 32, 10, C_WATCH_RING);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("TOCA UN INCIDENTE PARA VER DETALLE  •  [VOLVER]", 18, 216);
    _gfx->endWrite();
    _bgCleared = true;
  }

  void drawTicketDetailScreen(const TicketItem &ticket) {
    if (_bgCleared) return; // Draw once -> Zero flicker!
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    _gfx->fillRoundRect(4, 2, 312, 26, 8, C_WATCH_CARD);
    _gfx->drawRoundRect(4, 2, 312, 26, 8, C_DATA_ORANGE);
    _gfx->setTextColor(C_AMBER_GOLD);
    _gfx->setTextSize(1);
    _gfx->drawString("< LISTA", 12, 11);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(String("INCIDENT #") + ticket.id, 114, 11);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("RELOJ >", 254, 11);

    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(ticket.title.substring(0, 46), 14, 38);

    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString(String("Por: ") + ticket.requester + " | Prio: " + ticket.priority, 14, 54);

    _gfx->fillRoundRect(10, 70, 300, 124, 10, C_WATCH_CARD);
    _gfx->drawRoundRect(10, 70, 300, 124, 10, C_WATCH_RING);
    _gfx->setTextColor(C_GLASS_SHINE);

    String desc = ticket.description;
    if (desc.length() == 0) desc = "Incidente activo monitoreado en tiempo real por Dataholics Smartwatch OS.";
    int lineY = 82;
    for (int p = 0; p < (int)desc.length() && lineY < 182; p += 44) {
      _gfx->drawString(desc.substring(p, min((int)desc.length(), p + 44)), 18, lineY);
      lineY += 14;
    }

    _gfx->fillRoundRect(10, 202, 144, 32, 10, C_WATCH_CARD);
    _gfx->drawRoundRect(10, 202, 144, 32, 10, C_AMBER_GOLD);
    _gfx->setTextColor(C_AMBER_GOLD);
    _gfx->drawString("< LISTA TICKETS", 30, 214);

    _gfx->fillRoundRect(166, 202, 144, 32, 10, C_WATCH_CARD);
    _gfx->drawRoundRect(166, 202, 144, 32, 10, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("VOLVER AL RELOJ", 188, 214);
    _gfx->endWrite();
    _bgCleared = true;
  }

  void drawEnrollmentScreen(const String &siliconMac, const String &statusText) {
    if (_bgCleared) return; // Draw once -> Zero flicker!
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);
    _gfx->drawRoundRect(8, 8, 304, 224, 16, C_NEON_CYAN);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->setTextSize(2);
    _gfx->drawString("SMARTWATCH ENCLAVE", 42, 24);

    _gfx->setTextColor(C_TEXT_MUTED);
    _gfx->setTextSize(1);
    _gfx->drawString("HARDWARE SILICON eFUSE MAC:", 34, 58);

    _gfx->fillRoundRect(28, 72, 264, 32, 8, C_WATCH_CARD);
    _gfx->drawRoundRect(28, 72, 264, 32, 8, C_NEON_CYAN);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->setTextSize(2);
    _gfx->drawString(siliconMac, 42, 80);

    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString(String("Estado de Red: ") + statusText, 34, 120);

    _gfx->fillRoundRect(28, 166, 264, 34, 10, C_WATCH_CARD);
    _gfx->drawRoundRect(28, 166, 264, 34, 10, C_DATA_ORANGE);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->drawString("[ TOCAR PARA CONFIGURAR WI-FI ]", 54, 179);

    _gfx->setTextColor(C_TEXT_MUTED);
    _gfx->drawString("Toca arriba para volver a la caratula", 48, 210);
    _gfx->endWrite();
    _bgCleared = true;
  }

private:
  void _drawSingleEye(int cx, int cy, int w, int h, bool isLeft, uint16_t accent, float parallaxMult) {
    int breathY = (int)(sin(_animTick * 0.08f) * 2.0f);
    int gx = (int)(_gazeX * 15.0f * parallaxMult);
    int gy = (int)(_gazeY * 10.0f * parallaxMult) + breathY;
    int x = cx + gx;
    int y = cy + gy;

    // 1. Outer Depth Halo
    _canvas.drawRoundRect(x - w / 2 - 3, y - h / 2 - 3, w + 6, h + 6, 17, 0x10A2);
    _canvas.drawRoundRect(x - w / 2 - 1, y - h / 2 - 1, w + 2, h + 2, 16, C_CYAN_DIM);

    // 2. Main Glowing Eye Sclera/Visor
    _canvas.fillRoundRect(x - w / 2, y - h / 2, w, h, 15, accent);

    // 3. Subtle Horizontal Scanlines inside Eye
    for (int sy = y - h / 2 + 4; sy < y + h / 2 - 4; sy += 4) {
      _canvas.drawFastHLine(x - w / 2 + 5, sy, w - 10, 0x0842);
    }

    // 4. 3D Spherical Pupil & Iris (1.4x extra parallax for depth)
    int px = x + (int)(_gazeX * 9.0f * parallaxMult);
    int py = y + (int)(_gazeY * 6.0f * parallaxMult);
    int pRad = (int)(14.0f * _pupilScale);

    _canvas.fillCircle(px, py, pRad, C_VOID_BG);
    _canvas.drawCircle(px, py, (pRad * 2) / 3, C_WHITE_GLOW);

    // Dual Specular Glass Catchlights
    _canvas.fillCircle(px - pRad / 3 - 1, py - pRad / 3 - 1, 4, C_WHITE_GLOW);
    _canvas.fillCircle(px + pRad / 3 + 1, py + pRad / 3 + 1, 2, C_GLASS_SHINE);

    // 5. Expressive Upper Eyelid Mask
    int lidTop = y - h / 2 - 2;
    int lidW = w + 6;
    int leftX = x - w / 2 - 3;
    int rightX = x + w / 2 + 3;

    if (_upperLid > 0.03f || abs(_browAngle) > 0.05f) {
      int baseDrop = (int)(_upperLid * (float)h);
      int slantDrop = (int)(sin(_browAngle) * (w * 0.45f));
      int dropL = isLeft ? (baseDrop - slantDrop) : (baseDrop + slantDrop);
      int dropR = isLeft ? (baseDrop + slantDrop) : (baseDrop - slantDrop);
      dropL = constrain(dropL, 0, h + 4);
      dropR = constrain(dropR, 0, h + 4);

      _canvas.fillTriangle(leftX, lidTop, rightX, lidTop, rightX, lidTop + dropR, C_VOID_BG);
      _canvas.fillTriangle(leftX, lidTop, rightX, lidTop + dropR, leftX, lidTop + dropL, C_VOID_BG);
    }

    // 6. Expressive Lower Eyelid / Happy Crescent Mask (^ ^)
    if (_lowerLid > 0.05f) {
      int rise = (int)(_lowerLid * (float)h);
      _canvas.fillCircle(x, y + h / 2 + (h / 3) - rise / 2, (w * 3) / 5, C_VOID_BG);
      _canvas.fillRect(leftX, y + h / 2 - rise / 2, lidW, rise + 6, C_VOID_BG);
    }

    // 7. Floating Expressive Eyebrow Plate
    int bx = cx + (int)(_browX * 18.0f * parallaxMult);
    int by = cy - h / 2 - 10 - (int)_browLift + (int)(_browY * 7.0f * parallaxMult) + breathY;
    float ang = isLeft ? _browAngle : -_browAngle;
    int bw = (int)(w * 0.42f);
    int dy = (int)(sin(ang) * bw);

    for (int t = -2; t <= 2; t++) {
      _canvas.drawLine(bx - bw, by - dy + t, bx + bw, by + dy + t, accent);
    }
  }
};
