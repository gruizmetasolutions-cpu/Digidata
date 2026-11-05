#pragma once

#include <Arduino.h>
#include <time.h>
#include "cyd_display.h"
#include "pet_renderer.h" // Reuses PetMood and TicketItem types

// ============================================================================
// Dataholics DigiVice — Sovereign Mecha-Face & Smartwatch OS v4.0
// Target: ESP32-2432S028 (CYD 320x240 16-bit RGB565)
// Visual Identity: Dataholics Orange (#FF7300), TiRA Cyan (#00FFFF), Executive Gold
// Character Design:
//   - Aggressive angular Mecha/Predator optics (NO round anime bubbles)
//   - Heavy armored V-slanted brow plates with cocky asymmetric expressions
//   - Rock-steady Tactical Mecha Jawplate & Reactor Grille (NO trembling wave)
//   - 100% Flicker-Free RAM Sprite Double-Buffering (_canvas + _banner)
// ============================================================================

class CyberFaceRenderer {
private:
  LGFX *_gfx;
  LGFX_Sprite _canvas;
  LGFX_Sprite _banner;
  bool _spriteReady;
  bool _bannerReady;
  bool _bgCleared;

  // Mecha Optic & Armor Physics State
  uint32_t _animTick;
  float _gazeX, _gazeY;
  float _targetGazeX, _targetGazeY;
  float _browX, _browY;
  float _blink;
  uint32_t _nextBlinkTick;
  float _upperLidL, _upperLidR;
  float _lowerLidL, _lowerLidR;
  float _browAngleL, _browAngleR;
  float _browLiftL, _browLiftR;
  float _reticleScale;

  int _screenShakeX;
  int _screenShakeY;

  uint32_t _idleTimer;
  IdleSubState _idleSubState;
  bool _touchActive;

  // Dataholics Official Color Palette (RGB565)
  static constexpr uint16_t C_VOID_BG     = 0x0000; // Pure OLED Pitch Black
  static constexpr uint16_t C_CARBON_CARD = 0x0862; // Dark Tactical Carbon (#080D14)
  static constexpr uint16_t C_ARMOR_DARK  = 0x10A3; // Mecha Chassis Dark Slate
  static constexpr uint16_t C_ARMOR_EDGE  = 0x2965; // Titanium Bezel Edge
  static constexpr uint16_t C_DATA_ORANGE = 0xFB80; // Dataholics Signature Orange (#FF7300)
  static constexpr uint16_t C_ORANGE_DIM  = 0x6180; // Deep Hazard Bronze
  static constexpr uint16_t C_TIRA_CYAN   = 0x07FF; // TiRA Autonomous Electric Cyan
  static constexpr uint16_t C_CYAN_DIM    = 0x03EF; // Deep Cyber Teal
  static constexpr uint16_t C_EXEC_GOLD   = 0xFD20; // Executive Gold (#F59E0B)
  static constexpr uint16_t C_ALERT_RED   = 0xF924; // Crimson Overdrive Alert
  static constexpr uint16_t C_MINT_GREEN  = 0x2FE8; // Sovereign Emerald
  static constexpr uint16_t C_WHITE_PURE  = 0xFFFF; // Pure Crisp White
  static constexpr uint16_t C_TEXT_MUTED  = 0x8430; // Muted Technical Label

  // Central Sprite Geometry (220x140 = 61.6 KB, safe in ESP32 internal DRAM)
  static constexpr int FACE_W = 220;
  static constexpr int FACE_H = 140;
  static constexpr int FACE_X = 50;  // (320 - 220) / 2
  static constexpr int FACE_Y = 48;  // Between Top Bar (0..36) and Bottom Dock (204..240)

public:
  CyberFaceRenderer(LGFX *gfx)
      : _gfx(gfx), _canvas(gfx), _banner(gfx),
        _spriteReady(false), _bannerReady(false), _bgCleared(false),
        _animTick(0), _gazeX(0), _gazeY(0), _targetGazeX(0), _targetGazeY(0),
        _browX(0), _browY(0), _blink(0), _nextBlinkTick(90),
        _upperLidL(0.18f), _upperLidR(0.18f), _lowerLidL(0.08f), _lowerLidR(0.08f),
        _browAngleL(0.26f), _browAngleR(-0.26f), _browLiftL(0.0f), _browLiftR(0.0f),
        _reticleScale(1.0f), _screenShakeX(0), _screenShakeY(0),
        _idleTimer(0), _idleSubState(IDLE_REST), _touchActive(false) {}

  void begin() {
    _canvas.setColorDepth(16);
    if (_canvas.createSprite(FACE_W, FACE_H) != nullptr) {
      _spriteReady = true;
    } else {
      _canvas.setColorDepth(8);
      if (_canvas.createSprite(FACE_W, FACE_H) != nullptr) {
        _spriteReady = true;
      }
    }

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
      _targetGazeX = constrain((tx - 160.0f) / 100.0f, -1.0f, 1.0f);
      _targetGazeY = constrain((ty - 118.0f) / 65.0f, -1.0f, 1.0f);
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
  }

  void clearScreen() {
    _gfx->fillScreen(C_VOID_BG);
    _bgCleared = false;
  }

  void getFormattedTime(String &outHM, String &outSec, String &outDate) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
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
      unsigned long totalSec = (millis() / 1000) + (11 * 3600) + (25 * 60);
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

  // Helper: Draw a small Dataholics Split-Wing Emblem on any sprite/target
  template <typename T>
  void _drawMiniDataholicsEmblem(T &dst, int cx, int cy, int scale, uint16_t colWing, uint16_t colCut) {
    int wOuter = 11 * scale;
    int wInner = 2 * scale;
    int hOuter = 5 * scale;
    int hInner = 7 * scale;
    // Left Wing
    dst.fillTriangle(cx - wOuter, cy - hOuter, cx - wInner, cy - hInner, cx - wInner, cy + hInner, colWing);
    dst.fillTriangle(cx - wOuter, cy - hOuter, cx - wInner, cy + hInner, cx - wOuter, cy + hOuter, colWing);
    // Right Wing
    dst.fillTriangle(cx + wInner, cy - hInner, cx + wOuter, cy - hOuter, cx + wOuter, cy + hOuter, colWing);
    dst.fillTriangle(cx + wInner, cy - hInner, cx + wOuter, cy + hOuter, cx + wInner, cy + hInner, colWing);
    // Center Cutouts
    dst.fillCircle(cx - 6 * scale, cy, 3 * scale, colCut);
    dst.fillCircle(cx + 6 * scale, cy, 3 * scale, colCut);
    dst.fillRect(cx - 6 * scale, cy - scale, 12 * scale, 2 * scale + 1, colCut);
  }

  // ==========================================================================
  // EXECUTIVE DATAHOLICS BOOT ANIMATION (~2.5 Seconds, Audible + Zero Flicker)
  // ==========================================================================
  void playBootAnimation(void (*audioCb)(int, int)) {
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    // Tactical Dataholics Hazard Header & Footer Strips
    _gfx->fillRect(0, 0, 320, 18, 0x1860);
    _gfx->fillRect(0, 18, 320, 2, C_DATA_ORANGE);
    _gfx->fillRect(0, 222, 320, 18, 0x1860);
    _gfx->fillRect(0, 220, 320, 2, C_DATA_ORANGE);

    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->setTextSize(1);
    _gfx->drawString("/// DATAHOLICS SOVEREIGN BIOS v4.0 // IGNITION ///", 12, 5);
    _gfx->setTextColor(C_TIRA_CYAN);
    _gfx->drawString("TiRA AUTONOMOUS CORE // HARDWARE VERIFIED", 34, 226);

    // Draw Iconic Dataholics Split-Wing Orange Emblem (cx=160, cy=68)
    _drawMiniDataholicsEmblem(*_gfx, 160, 68, 4, C_DATA_ORANGE, C_VOID_BG);

    // Brand Typography
    _gfx->setTextColor(C_WHITE_PURE);
    _gfx->setTextSize(2);
    _gfx->drawString("D A T A H O L I C S", 46, 112);

    _gfx->setTextColor(C_TIRA_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString("SOVEREIGN MECHA WATCH OS", 86, 134);

    // Executive Clearance Box
    _gfx->fillRoundRect(44, 148, 232, 24, 4, C_CARBON_CARD);
    _gfx->drawRoundRect(44, 148, 232, 24, 4, C_EXEC_GOLD);
    _gfx->setTextColor(C_EXEC_GOLD);
    _gfx->drawString("* ALONSO RUIZ // CEO EDITION *", 70, 156);

    // Segmented Progress Frame (drawn once -> zero flicker)
    _gfx->drawRect(38, 184, 244, 16, C_ARMOR_EDGE);
    _gfx->endWrite();

    // 16-Segment Mecha Ignition Bar + Sovereign Startup Fanfare (~2.4s)
    const int bootNotes[16] = {
      440, 0, 554, 0, 659, 0, 880, 0,
      1108, 0, 1318, 0, 1760, 0, 2217, 2637
    };
    for (int s = 0; s < 16; s++) {
      uint16_t segCol = (s >= 12) ? C_TIRA_CYAN : C_DATA_ORANGE;
      _gfx->fillRect(42 + s * 15, 187, 12, 10, segCol);
      if (audioCb && bootNotes[s] > 0) {
        audioCb(bootNotes[s], (s == 15) ? 90 : 32);
      }
      delay(95);
    }

    delay(150);
    _gfx->fillScreen(C_VOID_BG);
    _bgCleared = false;
  }

  // ==========================================================================
  // CARATULA 1: SOVEREIGN MECHA-FACE (Aggressive Character, Zero Flicker)
  // ==========================================================================
  void renderFrame(PetMood mood, const String &ticketTitle, int ticketId, const String &ticketStatus, int activeCount, int resolvedCount, const String &deviceMac) {
    _animTick++;
    _idleTimer++;

    // Dataholics Tactical Color Mapping per Mood
    uint16_t opticCol = C_TIRA_CYAN;
    uint16_t armorCol = C_DATA_ORANGE;
    if (mood == MOOD_ALARM) {
      opticCol = C_ALERT_RED;
      armorCol = C_DATA_ORANGE;
    } else if (mood == MOOD_LAUGH) {
      opticCol = C_EXEC_GOLD;
      armorCol = C_DATA_ORANGE;
    } else if (mood == MOOD_CHOMP) {
      opticCol = C_MINT_GREEN;
      armorCol = C_EXEC_GOLD;
    } else if (mood == MOOD_POKED) {
      opticCol = C_DATA_ORANGE;
      armorCol = C_TIRA_CYAN;
    }

    // Draw static Mecha Helmet Side Rails ONCE when entering screen (0% flicker!)
    if (!_bgCleared) {
      _gfx->startWrite();
      _gfx->fillScreen(C_VOID_BG);

      // Left Mecha Cheek / Temple Armor Frame
      _gfx->drawLine(14, 46, 38, 46, C_DATA_ORANGE);
      _gfx->drawLine(14, 46, 8, 64, C_DATA_ORANGE);
      _gfx->drawLine(8, 64, 8, 166, C_ARMOR_EDGE);
      _gfx->drawLine(8, 166, 22, 188, C_DATA_ORANGE);
      _gfx->drawLine(22, 188, 42, 188, C_DATA_ORANGE);

      // Right Mecha Cheek / Temple Armor Frame
      _gfx->drawLine(282, 46, 306, 46, C_DATA_ORANGE);
      _gfx->drawLine(306, 46, 312, 64, C_DATA_ORANGE);
      _gfx->drawLine(312, 64, 312, 166, C_ARMOR_EDGE);
      _gfx->drawLine(312, 166, 298, 188, C_DATA_ORANGE);
      _gfx->drawLine(298, 188, 278, 188, C_DATA_ORANGE);

      // Side Tactical Vent Ticks
      for (int vy = 76; vy <= 156; vy += 16) {
        _gfx->fillRect(14, vy, 12, 4, C_ORANGE_DIM);
        _gfx->fillRect(294, vy, 12, 4, C_ORANGE_DIM);
      }

      _gfx->endWrite();
      _bgCleared = true;
    }

    // Deliberate, Predatory Saccades (locks onto targets sharply, holds steady)
    if (!_touchActive) {
      if (_animTick % 80 == 0) {
        int r = random(0, 100);
        if (r < 45) {
          // Center lock
          _targetGazeX = 0.0f;
          _targetGazeY = 0.0f;
        } else {
          // Sharp tactical scan
          _targetGazeX = (random(-10, 11) / 10.0f) * 0.85f;
          _targetGazeY = (random(-8, 9) / 10.0f) * 0.45f;
        }
      }
    }

    // Crisp mechanical shutter blink
    if (_animTick >= _nextBlinkTick) {
      _blink = 1.0f;
      _nextBlinkTick = _animTick + 100 + random(0, 120);
    }
    _blink += (0.0f - _blink) * 0.38f;

    // Fast, snappy predator eye tracking (no floaty anime lag)
    _gazeX += (_targetGazeX - _gazeX) * 0.32f;
    _gazeY += (_targetGazeY - _gazeY) * 0.32f;
    _browX += (_gazeX - _browX) * 0.18f;
    _browY += (_gazeY - _browY) * 0.18f;

    // ========================================================================
    // Aggressive Character Expression Presets (No Anime Wide-Eyed Look!)
    // ========================================================================
    // Default IDLE ("SOBERANO"): Confident, narrowed hunter glare with sharp V-brows
    float tUpL = 0.22f, tUpR = 0.22f;
    float tLoL = 0.10f, tLoR = 0.10f;
    float tAngL = 0.28f, tAngR = -0.28f; // Natural aggressive V-slant (\ /)
    float tLiftL = -2.0f, tLiftR = -2.0f;
    float tRet = 1.0f;

    if (mood == MOOD_ALARM) {
      // COMBATE: Steep lethal V-brow, narrowed combat slits, contracted diamond reticle
      tUpL = 0.38f; tUpR = 0.38f;
      tLoL = 0.18f; tLoR = 0.18f;
      tAngL = 0.48f; tAngR = -0.48f;
      tLiftL = -6.0f; tLiftR = -6.0f;
      tRet = 0.70f;
    } else if (mood == MOOD_LAUGH) {
      // SOBERBIO (Cocky Asymmetric Attitude): Left eye squinted, Right brow cocked high!
      tUpL = 0.42f; tUpR = 0.08f;
      tLoL = 0.28f; tLoR = 0.14f;
      tAngL = 0.32f; tAngR = -0.10f;
      tLiftL = -4.0f; tLiftR = 7.0f;
      tRet = 1.05f;
    } else if (mood == MOOD_CHOMP) {
      // OVERDRIVE: Blazing Sovereign power mode
      tUpL = 0.12f; tUpR = 0.12f;
      tLoL = 0.22f; tLoR = 0.22f;
      tAngL = 0.36f; tAngR = -0.36f;
      tLiftL = 3.0f; tLiftR = 3.0f;
      tRet = 1.25f;
    } else if (mood == MOOD_POKED) {
      // LOCK-ON: Target acquired snap
      tUpL = 0.32f; tUpR = 0.32f;
      tLoL = 0.22f; tLoR = 0.22f;
      tAngL = 0.42f; tAngR = -0.42f;
      tLiftL = -5.0f; tLiftR = -5.0f;
      tRet = 0.65f;
    }

    _upperLidL += (max(tUpL, _blink) - _upperLidL) * 0.32f;
    _upperLidR += (max(tUpR, _blink) - _upperLidR) * 0.32f;
    _lowerLidL += (tLoL - _lowerLidL) * 0.28f;
    _lowerLidR += (tLoR - _lowerLidR) * 0.28f;
    _browAngleL += (tAngL - _browAngleL) * 0.26f;
    _browAngleR += (tAngR - _browAngleR) * 0.26f;
    _browLiftL += (tLiftL - _browLiftL) * 0.26f;
    _browLiftR += (tLiftR - _browLiftR) * 0.26f;
    _reticleScale += (tRet - _reticleScale) * 0.28f;

    String strHM, strSec, strDate;
    getFormattedTime(strHM, strSec, strDate);

    // ------------------------------------------------------------------------
    // 1. TOP DATAHOLICS WATCH BAR (y = 0..35) -> Double-buffered in _banner
    // ------------------------------------------------------------------------
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);

      // Angular Tactical Top Bar
      _banner.fillRoundRect(4, 2, 312, 30, 8, C_CARBON_CARD);
      _banner.drawRoundRect(4, 2, 312, 30, 8, C_ARMOR_EDGE);
      _banner.drawFastHLine(16, 2, 288, C_DATA_ORANGE);

      // Left Button: Switch to Smartwatch Mode
      _banner.fillRoundRect(8, 6, 88, 22, 6, C_ARMOR_DARK);
      _banner.drawRoundRect(8, 6, 88, 22, 6, C_DATA_ORANGE);
      _banner.setTextColor(C_DATA_ORANGE);
      _banner.setTextSize(1);
      _banner.drawString("RELOJ OS >", 18, 13);

      // Center: Live Time HH:MM:SS
      _banner.setTextColor(C_WHITE_PURE);
      _banner.setTextSize(2);
      _banner.drawString(strHM, 116, 9);
      _banner.setTextColor(C_DATA_ORANGE);
      _banner.setTextSize(1);
      _banner.drawString(strSec, 178, 14);

      // Right Pill: ServiceDesk Status
      uint16_t actCol = (activeCount > 0) ? C_ALERT_RED : C_MINT_GREEN;
      _banner.fillRoundRect(216, 6, 96, 22, 6, C_ARMOR_DARK);
      _banner.drawRoundRect(216, 6, 96, 22, 6, actCol);
      _banner.setTextColor(C_WHITE_PURE);
      _banner.drawString(String("ACT:") + activeCount + " OK:" + resolvedCount, 226, 13);

      _banner.pushSprite(_gfx, 0, 0);
    }

    // ------------------------------------------------------------------------
    // 2. CENTER MECHA VISOR (220x140) -> Double-buffered in _canvas
    // ------------------------------------------------------------------------
    if (_spriteReady) {
      _canvas.fillScreen(C_VOID_BG);

      int gx = (int)(_gazeX * 12.0f);
      int gy = (int)(_gazeY * 7.0f);

      // A. Forehead Dataholics Split-Wing Crest (Moves with subtle 0.5x parallax)
      int crestX = (FACE_W / 2) + (gx / 2);
      int crestY = 14 + (gy / 3);
      _canvas.drawFastHLine(crestX - 44, crestY, 22, C_ARMOR_EDGE);
      _canvas.drawFastHLine(crestX + 22, crestY, 22, C_ARMOR_EDGE);
      _drawMiniDataholicsEmblem(_canvas, crestX, crestY, 1, C_DATA_ORANGE, C_VOID_BG);

      // B. Left & Right Aggressive Mecha Optics (cx=56, cy=58 and cx=164, cy=58)
      _drawMechaOptic(56, 58, 82, 42, true, opticCol, armorCol);
      _drawMechaOptic(164, 58, 82, 42, false, opticCol, armorCol);

      // C. Center Nasal Bridge / Targeting Sensor
      int bridgeX = (FACE_W / 2) + gx;
      int bridgeY = 62 + gy;
      _canvas.drawLine(bridgeX - 4, bridgeY - 6, bridgeX, bridgeY - 10, C_ARMOR_EDGE);
      _canvas.drawLine(bridgeX + 4, bridgeY - 6, bridgeX, bridgeY - 10, C_ARMOR_EDGE);
      _canvas.fillTriangle(bridgeX - 3, bridgeY + 4, bridgeX + 3, bridgeY + 4, bridgeX, bridgeY + 10, C_DATA_ORANGE);

      // D. Rock-Steady Tactical Mecha Jawplate & Reactor Grille (NO trembling wave!)
      int jx = (FACE_W / 2) + (int)(_gazeX * 7.0f);
      int jy = 114 + (int)(_gazeY * 4.0f);

      // Cheek Armor Chassis Lines (\_______/)
      _canvas.drawLine(jx - 78, jy - 14, jx - 46, jy + 12, C_ARMOR_EDGE);
      _canvas.drawLine(jx - 77, jy - 14, jx - 45, jy + 12, C_DATA_ORANGE);
      _canvas.drawLine(jx + 78, jy - 14, jx + 46, jy + 12, C_ARMOR_EDGE);
      _canvas.drawLine(jx + 77, jy - 14, jx + 45, jy + 12, C_DATA_ORANGE);
      _canvas.drawFastHLine(jx - 45, jy + 12, 90, C_ARMOR_EDGE);

      if (mood == MOOD_LAUGH) {
        // SOBERBIO: Sharp, cocky asymmetric Mecha Smirk (angled up on the right!)
        _canvas.drawLine(jx - 30, jy + 2, jx + 6, jy + 2, C_EXEC_GOLD);
        _canvas.drawLine(jx - 30, jy + 3, jx + 6, jy + 3, C_EXEC_GOLD);
        _canvas.drawLine(jx + 6, jy + 2, jx + 32, jy - 9, C_DATA_ORANGE);
        _canvas.drawLine(jx + 6, jy + 3, jx + 32, jy - 8, C_DATA_ORANGE);
        _canvas.drawLine(jx + 22, jy - 11, jx + 34, jy - 6, C_WHITE_PURE); // Fang highlight
      } else if (mood == MOOD_ALARM) {
        // COMBATE: Interlocking Crimson Mecha Fangs / High-Alert Vents
        _canvas.fillRoundRect(jx - 38, jy - 7, 76, 14, 3, 0x2000);
        _canvas.drawRoundRect(jx - 38, jy - 7, 76, 14, 3, C_ALERT_RED);
        for (int fx = -30; fx <= 24; fx += 12) {
          _canvas.fillTriangle(jx + fx, jy - 6, jx + fx + 8, jy - 6, jx + fx + 4, jy + 5, C_ALERT_RED);
          _canvas.drawFastVLine(jx + fx + 4, jy - 5, 8, C_DATA_ORANGE);
        }
      } else if (mood == MOOD_CHOMP) {
        // OVERDRIVE: Blazing Sovereign Emerald & Gold Power Grille
        _canvas.fillRoundRect(jx - 40, jy - 7, 80, 14, 3, C_CARBON_CARD);
        _canvas.drawRoundRect(jx - 40, jy - 7, 80, 14, 3, C_EXEC_GOLD);
        for (int vx = -32; vx <= 28; vx += 10) {
          _canvas.fillRect(jx + vx, jy - 4, 6, 8, C_MINT_GREEN);
        }
      } else {
        // SOBERANO (IDLE) & LOCK-ON: Rock-steady 7-slit Armored Faceplate Vents!
        _canvas.fillRoundRect(jx - 38, jy - 6, 76, 12, 3, C_CARBON_CARD);
        _canvas.drawRoundRect(jx - 38, jy - 6, 76, 12, 3, C_ARMOR_EDGE);
        for (int i = -3; i <= 3; i++) {
          int vx = jx + i * 9;
          uint16_t vCol = (abs(i) <= 1) ? C_DATA_ORANGE : opticCol;
          // Slanted armor vent slit
          _canvas.fillRect(vx - 2, jy - 3, 5, 6, vCol);
        }
      }

      _canvas.pushSprite(_gfx, FACE_X + _screenShakeX, FACE_Y + _screenShakeY);
    }

    // ------------------------------------------------------------------------
    // 3. BOTTOM DATAHOLICS SMART DOCK (y = 204..239) -> Double-buffered in _banner
    // ------------------------------------------------------------------------
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);

      // Left Pill: Executive Badge
      _banner.fillRoundRect(4, 4, 98, 28, 8, C_CARBON_CARD);
      _banner.drawRoundRect(4, 4, 98, 28, 8, C_EXEC_GOLD);
      _banner.setTextColor(C_EXEC_GOLD);
      _banner.setTextSize(1);
      _banner.drawString("ALONSO CEO", 19, 14);

      // Center Pill: Character Attitude Selector
      _banner.fillRoundRect(107, 4, 106, 28, 8, C_CARBON_CARD);
      _banner.drawRoundRect(107, 4, 106, 28, 8, C_DATA_ORANGE);
      _banner.setTextColor(C_DATA_ORANGE);
      const char *moodLabel = "MODO: SOBERANO";
      if (mood == MOOD_ALARM) moodLabel = "MODO: COMBATE";
      else if (mood == MOOD_LAUGH) moodLabel = "MODO: SOBERBIO";
      else if (mood == MOOD_CHOMP) moodLabel = "MODO: OVERDRIVE";
      else if (mood == MOOD_POKED) moodLabel = "MODO: LOCK-ON";
      _banner.drawString(moodLabel, 116, 14);

      // Right Pill: Incident Stream
      _banner.fillRoundRect(218, 4, 98, 28, 8, C_CARBON_CARD);
      _banner.drawRoundRect(218, 4, 98, 28, 8, C_TIRA_CYAN);
      _banner.setTextColor(C_TIRA_CYAN);
      _banner.drawString("TICKETS >", 238, 14);

      _banner.pushSprite(_gfx, 0, 204);
    }
  }

  // ==========================================================================
  // CARATULA 2: DATAHOLICS SMARTWATCH CHRONOGRAPH (Zero Flicker)
  // ==========================================================================
  void drawSmartwatchScreen(PetMood mood, const String &ticketTitle, int ticketId, int activeCount, int resolvedCount, bool wifiOnline, int rssi) {
    _animTick++;

    uint16_t opticCol = C_TIRA_CYAN;
    if (mood == MOOD_ALARM) opticCol = C_ALERT_RED;
    else if (mood == MOOD_LAUGH) opticCol = C_EXEC_GOLD;
    else if (mood == MOOD_CHOMP) opticCol = C_MINT_GREEN;

    if (_animTick >= _nextBlinkTick) {
      _blink = 1.0f;
      _nextBlinkTick = _animTick + 95 + random(0, 100);
    }
    _blink += (0.0f - _blink) * 0.35f;

    // Draw side watch complication rails ONCE when entering Smartwatch mode
    if (!_bgCleared) {
      _gfx->startWrite();
      _gfx->fillScreen(C_VOID_BG);

      // Left Rail: TiRA Core Power Gauge
      _gfx->fillRoundRect(6, 46, 38, 144, 10, C_CARBON_CARD);
      _gfx->drawRoundRect(6, 46, 38, 144, 10, C_ARMOR_EDGE);
      _gfx->drawFastHLine(12, 46, 26, C_DATA_ORANGE);
      _gfx->setTextColor(C_DATA_ORANGE);
      _gfx->setTextSize(1);
      _gfx->drawString("CORE", 13, 56);
      for (int s = 0; s < 6; s++) {
        uint16_t sCol = (s == 0) ? C_TIRA_CYAN : C_DATA_ORANGE;
        _gfx->fillRect(15, 74 + s * 14, 20, 9, sCol);
      }
      _gfx->setTextColor(C_WHITE_PURE);
      _gfx->drawString("100%", 13, 172);

      // Right Rail: Uplink Signal Bars
      _gfx->fillRoundRect(276, 46, 38, 144, 10, C_CARBON_CARD);
      _gfx->drawRoundRect(276, 46, 38, 144, 10, C_ARMOR_EDGE);
      _gfx->drawFastHLine(282, 46, 26, C_TIRA_CYAN);
      _gfx->setTextColor(C_TIRA_CYAN);
      _gfx->drawString("RED", 286, 56);
      uint16_t netCol = wifiOnline ? C_MINT_GREEN : C_DATA_ORANGE;
      _gfx->fillRect(283, 134, 4, 10, netCol);
      _gfx->fillRect(290, 124, 4, 20, netCol);
      _gfx->fillRect(297, 114, 4, 30, netCol);
      _gfx->fillRect(304, 102, 4, 42, wifiOnline ? netCol : C_ARMOR_DARK);
      _gfx->setTextColor(netCol);
      _gfx->drawString(wifiOnline ? "SYNC" : "LOC", 283, 172);

      _gfx->endWrite();
      _bgCleared = true;
    }

    String strHM, strSec, strDate;
    getFormattedTime(strHM, strSec, strDate);

    // 1. TOP SMARTWATCH BAR (y = 0..35) -> _banner
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);
      _banner.fillRoundRect(4, 2, 312, 30, 8, C_CARBON_CARD);
      _banner.drawRoundRect(4, 2, 312, 30, 8, C_ARMOR_EDGE);
      _banner.drawFastHLine(16, 2, 288, C_DATA_ORANGE);

      _banner.fillRoundRect(8, 6, 92, 22, 6, C_ARMOR_DARK);
      _banner.drawRoundRect(8, 6, 92, 22, 6, C_TIRA_CYAN);
      _banner.setTextColor(C_TIRA_CYAN);
      _banner.setTextSize(1);
      _banner.drawString("< ROSTRO", 22, 13);

      _banner.setTextColor(C_WHITE_PURE);
      _banner.drawString(strDate, 122, 13);

      uint16_t wfCol = wifiOnline ? C_MINT_GREEN : C_DATA_ORANGE;
      _banner.fillCircle(234, 17, 4, wfCol);
      _banner.setTextColor(wfCol);
      _banner.drawString(wifiOnline ? "WIFI ONLINE" : "CONFIG WIFI", 244, 13);

      _banner.pushSprite(_gfx, 0, 0);
    }

    // 2. CENTER SMARTWATCH DIAL (220x140) -> _canvas
    if (_spriteReady) {
      _canvas.fillScreen(C_VOID_BG);

      _canvas.fillRoundRect(0, 0, FACE_W, FACE_H, 12, C_CARBON_CARD);
      _canvas.drawRoundRect(0, 0, FACE_W, FACE_H, 12, C_ARMOR_EDGE);
      _canvas.drawFastHLine(16, 0, FACE_W - 32, C_DATA_ORANGE);

      // Dataholics Split-Wing Emblem + Digital Chronograph
      _drawMiniDataholicsEmblem(_canvas, 28, 26, 1, C_DATA_ORANGE, C_CARBON_CARD);

      _canvas.setTextColor(C_WHITE_PURE);
      _canvas.setTextSize(4);
      _canvas.drawString(strHM, 48, 12);

      // Seconds Badge
      _canvas.fillRoundRect(170, 12, 42, 18, 4, C_ARMOR_DARK);
      _canvas.drawRoundRect(170, 12, 42, 18, 4, C_DATA_ORANGE);
      _canvas.setTextColor(C_DATA_ORANGE);
      _canvas.setTextSize(1);
      _canvas.drawString(strSec, 180, 17);

      // Mini Slanted Mecha Guardian Eyes in top-right of Dial!
      if (_blink < 0.6f) {
        _canvas.fillTriangle(172, 36, 188, 39, 174, 43, opticCol);
        _canvas.fillTriangle(208, 36, 192, 39, 206, 43, opticCol);
      } else {
        _canvas.drawFastHLine(172, 40, 15, C_DATA_ORANGE);
        _canvas.drawFastHLine(193, 40, 15, C_DATA_ORANGE);
      }

      // Tactical Telemetry Pulse Line (y = 52)
      _canvas.drawFastHLine(12, 52, 196, C_ARMOR_DARK);
      int pulsePos = (_animTick * 5) % 192;
      _canvas.fillRect(12 + pulsePos, 51, 14, 3, C_DATA_ORANGE);

      // Complication 1: ACTIVOS (Left Card)
      uint16_t actCol = (activeCount > 0) ? C_ALERT_RED : C_TIRA_CYAN;
      _canvas.fillRoundRect(10, 62, 96, 50, 8, C_ARMOR_DARK);
      _canvas.drawRoundRect(10, 62, 96, 50, 8, actCol);
      _canvas.drawCircle(30, 87, 13, C_ARMOR_EDGE);
      _canvas.drawCircle(30, 87, 12, actCol);
      _canvas.fillRect(27, 84, 7, 7, C_DATA_ORANGE);
      _canvas.setTextColor(C_TEXT_MUTED);
      _canvas.setTextSize(1);
      _canvas.drawString("ACTIVOS", 50, 71);
      _canvas.setTextColor(C_WHITE_PURE);
      _canvas.setTextSize(2);
      _canvas.drawString(String(activeCount), 50, 86);

      // Complication 2: RESUELTOS (Right Card)
      _canvas.fillRoundRect(114, 62, 96, 50, 8, C_ARMOR_DARK);
      _canvas.drawRoundRect(114, 62, 96, 50, 8, C_MINT_GREEN);
      _canvas.drawCircle(134, 87, 13, C_ARMOR_EDGE);
      _canvas.drawCircle(134, 87, 12, C_MINT_GREEN);
      _canvas.fillRect(131, 84, 7, 7, C_MINT_GREEN);
      _canvas.setTextColor(C_TEXT_MUTED);
      _canvas.setTextSize(1);
      _canvas.drawString("RESUELTOS", 150, 71);
      _canvas.setTextColor(C_MINT_GREEN);
      _canvas.setTextSize(2);
      _canvas.drawString(String(resolvedCount), 152, 86);

      // Bottom Feed Line inside Dial
      _canvas.setTextColor(C_EXEC_GOLD);
      _canvas.setTextSize(1);
      String shortTkt = String("#") + ticketId + " " + ticketTitle;
      _canvas.drawString(shortTkt.substring(0, 32), 14, 122);

      _canvas.pushSprite(_gfx, FACE_X, FACE_Y);
    }

    // 3. BOTTOM SMARTWATCH DOCK (y = 204..239) -> _banner
    if (_bannerReady) {
      _banner.fillScreen(C_VOID_BG);

      _banner.fillRoundRect(4, 4, 98, 28, 8, C_CARBON_CARD);
      _banner.drawRoundRect(4, 4, 98, 28, 8, C_TIRA_CYAN);
      _banner.setTextColor(C_TIRA_CYAN);
      _banner.setTextSize(1);
      _banner.drawString("ROSTRO MECHA", 14, 14);

      _banner.fillRoundRect(107, 4, 106, 28, 8, C_CARBON_CARD);
      _banner.drawRoundRect(107, 4, 106, 28, 8, C_EXEC_GOLD);
      _banner.setTextColor(C_EXEC_GOLD);
      _banner.drawString("ALONSO CEO", 128, 14);

      _banner.fillRoundRect(218, 4, 98, 28, 8, C_CARBON_CARD);
      _banner.drawRoundRect(218, 4, 98, 28, 8, C_DATA_ORANGE);
      _banner.setTextColor(C_DATA_ORANGE);
      _banner.drawString("TICKETS >", 238, 14);

      _banner.pushSprite(_gfx, 0, 204);
    }
  }

  // ==========================================================================
  // SECONDARY SCREENS (Zero-Flicker Dataholics Executive Styling)
  // ==========================================================================
  void drawConferenceBadge(const String &nameFirst, const String &nameLast, const String &role, const String &mac) {
    _animTick++;

    if (!_bgCleared) {
      _gfx->startWrite();
      _gfx->fillScreen(C_VOID_BG);

      _gfx->drawRoundRect(6, 6, 308, 228, 12, C_DATA_ORANGE);
      _gfx->drawRoundRect(8, 8, 304, 224, 10, C_ARMOR_EDGE);

      _gfx->fillRoundRect(12, 12, 296, 24, 6, C_CARBON_CARD);
      _gfx->setTextColor(C_DATA_ORANGE);
      _gfx->setTextSize(1);
      _gfx->drawString("DATAHOLICS SOVEREIGN AI // VIP CREDENTIAL", 28, 20);

      _gfx->setTextSize(3);
      _gfx->setTextColor(C_WHITE_PURE);
      _gfx->drawString(nameFirst, 38, 122);
      _gfx->setTextColor(C_DATA_ORANGE);
      _gfx->drawString(nameLast, 172, 122);

      _gfx->fillRoundRect(32, 162, 256, 26, 6, C_CARBON_CARD);
      _gfx->drawRoundRect(32, 162, 256, 26, 6, C_EXEC_GOLD);
      _gfx->setTextColor(C_EXEC_GOLD);
      _gfx->setTextSize(1);
      _gfx->drawString("CEO  •  CHIEF EXECUTIVE OFFICER", 60, 171);

      _gfx->setTextColor(C_TEXT_MUTED);
      _gfx->drawString(String("ID: ") + mac + "  •  TOCA PARA VOLVER", 36, 206);
      _gfx->endWrite();
      _bgCleared = true;
    }

    // Animated Dataholics Emblem + Aggressive Mini-Optics in _banner at y=56
    if (_bannerReady) {
      if (_animTick >= _nextBlinkTick) {
        _blink = 1.0f;
        _nextBlinkTick = _animTick + 90 + random(0, 90);
      }
      _blink += (0.0f - _blink) * 0.35f;

      _banner.fillScreen(C_VOID_BG);
      _drawMiniDataholicsEmblem(_banner, 160, 18, 2, C_DATA_ORANGE, C_VOID_BG);

      if (_blink < 0.5f) {
        // Sharp predatory mini-optics flanking the Dataholics emblem
        _banner.fillTriangle(64, 10, 108, 18, 70, 26, C_TIRA_CYAN);
        _banner.fillTriangle(256, 10, 212, 18, 250, 26, C_TIRA_CYAN);
        _banner.drawLine(62, 7, 110, 15, C_DATA_ORANGE);
        _banner.drawLine(258, 7, 210, 15, C_DATA_ORANGE);
      } else {
        _banner.drawLine(64, 18, 108, 18, C_DATA_ORANGE);
        _banner.drawLine(212, 18, 256, 18, C_DATA_ORANGE);
      }
      _banner.pushSprite(_gfx, 0, 56);
    }
  }

  void drawTicketListScreen(const TicketItem tickets[], int count) {
    if (_bgCleared) return;
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    _gfx->fillRoundRect(4, 2, 312, 26, 6, C_CARBON_CARD);
    _gfx->drawRoundRect(4, 2, 312, 26, 6, C_DATA_ORANGE);
    _gfx->setTextColor(C_WHITE_PURE);
    _gfx->setTextSize(1);
    _gfx->drawString("DATAHOLICS // INCIDENTS STREAM", 14, 11);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->drawString("< VOLVER", 248, 11);

    for (int i = 0; i < 4; i++) {
      int y = 32 + i * 42;
      if (i < count) {
        _gfx->fillRoundRect(8, y, 304, 38, 6, C_CARBON_CARD);
        _gfx->drawRoundRect(8, y, 304, 38, 6, C_ARMOR_EDGE);

        uint16_t stColor = (tickets[i].status == "resolved" || tickets[i].status == "RESOLVED") ? C_MINT_GREEN :
                           ((tickets[i].status == "in_progress" || tickets[i].status == "UPDATED") ? C_EXEC_GOLD : C_COMBAT_RED);
        _gfx->fillRoundRect(10, y + 4, 5, 30, 2, stColor);

        _gfx->setTextColor(stColor);
        _gfx->drawString(String("#") + tickets[i].id + " [" + tickets[i].status + "] PRIO: " + tickets[i].priority, 22, y + 6);

        _gfx->setTextColor(C_WHITE_PURE);
        _gfx->drawString(tickets[i].title.substring(0, 42), 22, y + 21);
      } else {
        _gfx->fillRoundRect(8, y, 304, 38, 6, 0x0410);
        _gfx->drawRoundRect(8, y, 304, 38, 6, C_ARMOR_DARK);
        _gfx->setTextColor(C_TEXT_MUTED);
        _gfx->drawString("-- SIN INCIDENTES ACTIVOS --", 74, y + 15);
      }
    }

    _gfx->fillRoundRect(4, 204, 312, 32, 8, C_CARBON_CARD);
    _gfx->drawRoundRect(4, 204, 312, 32, 8, C_ARMOR_EDGE);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->drawString("TOCA UN INCIDENTE PARA VER DOSSIER  •  [VOLVER]", 18, 216);
    _gfx->endWrite();
    _bgCleared = true;
  }

  void drawTicketDetailScreen(const TicketItem &ticket) {
    if (_bgCleared) return;
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    _gfx->fillRoundRect(4, 2, 312, 26, 6, C_CARBON_CARD);
    _gfx->drawRoundRect(4, 2, 312, 26, 6, C_DATA_ORANGE);
    _gfx->setTextColor(C_EXEC_GOLD);
    _gfx->setTextSize(1);
    _gfx->drawString("< LISTA", 12, 11);
    _gfx->setTextColor(C_WHITE_PURE);
    _gfx->drawString(String("INCIDENT #") + ticket.id, 114, 11);
    _gfx->setTextColor(C_TIRA_CYAN);
    _gfx->drawString("RELOJ >", 254, 11);

    _gfx->setTextColor(C_WHITE_PURE);
    _gfx->drawString(ticket.title.substring(0, 46), 14, 38);

    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->drawString(String("Por: ") + ticket.requester + " | Prio: " + ticket.priority, 14, 54);

    _gfx->fillRoundRect(10, 70, 300, 124, 8, C_CARBON_CARD);
    _gfx->drawRoundRect(10, 70, 300, 124, 8, C_ARMOR_EDGE);
    _gfx->setTextColor(C_WHITE_PURE);

    String desc = ticket.description;
    if (desc.length() == 0) desc = "Incidente monitoreado por Dataholics Sovereign Smartwatch OS.";
    int lineY = 82;
    for (int p = 0; p < (int)desc.length() && lineY < 182; p += 44) {
      _gfx->drawString(desc.substring(p, min((int)desc.length(), p + 44)), 18, lineY);
      lineY += 14;
    }

    _gfx->fillRoundRect(10, 202, 144, 32, 8, C_CARBON_CARD);
    _gfx->drawRoundRect(10, 202, 144, 32, 8, C_EXEC_GOLD);
    _gfx->setTextColor(C_EXEC_GOLD);
    _gfx->drawString("< LISTA TICKETS", 30, 214);

    _gfx->fillRoundRect(166, 202, 144, 32, 8, C_CARBON_CARD);
    _gfx->drawRoundRect(166, 202, 144, 32, 8, C_DATA_ORANGE);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->drawString("VOLVER AL RELOJ", 188, 214);
    _gfx->endWrite();
    _bgCleared = true;
  }

  void drawEnrollmentScreen(const String &siliconMac, const String &statusText) {
    if (_bgCleared) return;
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);
    _gfx->drawRoundRect(8, 8, 304, 224, 12, C_DATA_ORANGE);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->setTextSize(2);
    _gfx->drawString("DATAHOLICS ENCLAVE", 42, 24);

    _gfx->setTextColor(C_TEXT_MUTED);
    _gfx->setTextSize(1);
    _gfx->drawString("HARDWARE SILICON eFUSE MAC:", 34, 58);

    _gfx->fillRoundRect(28, 72, 264, 32, 6, C_CARBON_CARD);
    _gfx->drawRoundRect(28, 72, 264, 32, 6, C_TIRA_CYAN);
    _gfx->setTextColor(C_WHITE_PURE);
    _gfx->setTextSize(2);
    _gfx->drawString(siliconMac, 42, 80);

    _gfx->setTextColor(C_TIRA_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString(String("Estado de Red: ") + statusText, 34, 120);

    _gfx->fillRoundRect(28, 166, 264, 34, 8, C_CARBON_CARD);
    _gfx->drawRoundRect(28, 166, 264, 34, 8, C_DATA_ORANGE);
    _gfx->setTextColor(C_DATA_ORANGE);
    _gfx->drawString("[ TOCAR PARA CONFIGURAR WI-FI ]", 54, 179);

    _gfx->setTextColor(C_TEXT_MUTED);
    _gfx->drawString("Toca arriba para volver a la caratula", 48, 210);
    _gfx->endWrite();
    _bgCleared = true;
  }

private:
  // ==========================================================================
  // AGGRESSIVE ANGULAR MECHA OPTIC RENDERER (Zero Anime Round Bubbles!)
  // ==========================================================================
  void _drawMechaOptic(int cx, int cy, int w, int h, bool isLeft, uint16_t opticCol, uint16_t armorCol) {
    int gx = (int)(_gazeX * 14.0f);
    int gy = (int)(_gazeY * 8.0f);
    int x = cx + gx;
    int y = cy + gy;

    int hw = w / 2;
    int hh = h / 2;

    // 1. Outer Recessed Armor Socket
    _canvas.fillRoundRect(x - hw - 2, y - hh - 2, w + 4, h + 4, 6, C_ARMOR_DARK);
    _canvas.drawRoundRect(x - hw - 2, y - hh - 2, w + 4, h + 4, 6, C_ARMOR_EDGE);

    // 2. Main Glowing Optic Lens
    _canvas.fillRoundRect(x - hw, y - hh, w, h, 4, opticCol);

    // 3. Tactical Horizontal Scanlines inside Lens
    for (int sy = y - hh + 3; sy < y + hh - 3; sy += 3) {
      _canvas.drawFastHLine(x - hw + 2, sy, w - 4, 0x0842);
    }

    // 4. Precision Tactical Targeting Core (Moves with 1.45x Parallax for 3D Depth!)
    //    Instead of a round anime pupil with cute bubbles, we render an angular
    //    Hex/Diamond Predator Core with a vertical slit & targeting ticks!
    int px = x + (int)(_gazeX * 9.0f);
    int py = y + (int)(_gazeY * 5.0f);
    int rSize = (int)(11.0f * _reticleScale);

    // Dark Obsidian Diamond Core
    _canvas.fillTriangle(px, py - rSize, px - rSize, py, px + rSize, py, C_VOID_BG);
    _canvas.fillTriangle(px, py + rSize, px - rSize, py, px + rSize, py, C_VOID_BG);

    // Glowing Inner Diamond Frame
    int ir = max(4, (rSize * 2) / 3);
    _canvas.drawLine(px, py - ir, px + ir, py, C_WHITE_PURE);
    _canvas.drawLine(px + ir, py, px, py + ir, C_WHITE_PURE);
    _canvas.drawLine(px, py + ir, px - ir, py, C_WHITE_PURE);
    _canvas.drawLine(px - ir, py, px, py - ir, C_WHITE_PURE);

    // Vertical Predator Slit / Lock-On Hairline in Center
    _canvas.fillRect(px - 1, py - ir + 2, 3, (ir * 2) - 3, armorCol);

    // Angular Visor Blade Slash Glint (sharp diagonal reflection at outer corner)
    int glintX = isLeft ? (x - hw + 8) : (x + hw - 16);
    _canvas.drawLine(glintX, y - hh + 4, glintX + 8, y - hh + 12, C_WHITE_PURE);
    _canvas.drawLine(glintX + 1, y - hh + 4, glintX + 9, y - hh + 12, C_WHITE_PURE);

    // 5. Outer Corner Chamfer Cutouts (gives the eye a sharp hexagonal mecha silhouette)
    _canvas.fillTriangle(x - hw - 2, y - hh - 2, x - hw + 10, y - hh - 2, x - hw - 2, y - hh + 10, C_VOID_BG);
    _canvas.fillTriangle(x + hw + 2, y - hh - 2, x + hw - 10, y - hh - 2, x + hw + 2, y - hh + 10, C_VOID_BG);
    _canvas.fillTriangle(x - hw - 2, y + hh + 2, x - hw + 10, y + hh + 2, x - hw - 2, y + hh - 10, C_VOID_BG);
    _canvas.fillTriangle(x + hw + 2, y + hh + 2, x + hw - 10, y + hh + 2, x + hw + 2, y + hh - 10, C_VOID_BG);

    // 6. Aggressive Slanted Upper Eyelid Armor Cut (\  /)
    float upperClosure = isLeft ? _upperLidL : _upperLidR;
    float browAng = isLeft ? _browAngleL : _browAngleR;
    int lidTop = y - hh - 3;
    int leftX = x - hw - 3;
    int rightX = x + hw + 3;

    int baseDrop = (int)(upperClosure * (float)h);
    int slantDrop = (int)(sin(browAng) * (w * 0.42f));
    int dropL = constrain(baseDrop - slantDrop, 0, h + 6);
    int dropR = constrain(baseDrop + slantDrop, 0, h + 6);

    _canvas.fillTriangle(leftX, lidTop, rightX, lidTop, rightX, lidTop + dropR, C_VOID_BG);
    _canvas.fillTriangle(leftX, lidTop, rightX, lidTop + dropR, leftX, lidTop + dropL, C_VOID_BG);

    // 7. Sharp Lower Cheek Armor Cut (gives fierce narrowed hunter glare)
    float lowerClosure = isLeft ? _lowerLidL : _lowerLidR;
    if (lowerClosure > 0.02f) {
      int lidBot = y + hh + 3;
      int rise = (int)(lowerClosure * (float)h);
      int riseInner = isLeft ? (rise + 4) : (rise - 2);
      int riseOuter = isLeft ? (rise - 2) : (rise + 4);
      _canvas.fillTriangle(leftX, lidBot, rightX, lidBot, rightX, lidBot - riseInner, C_VOID_BG);
      _canvas.fillTriangle(leftX, lidBot, rightX, lidBot - riseInner, leftX, lidBot - riseOuter, C_VOID_BG);
      // Glowing lower rim edge
      _canvas.drawLine(leftX + 4, lidBot - riseOuter, rightX - 4, lidBot - riseInner, C_ARMOR_EDGE);
    }

    // 8. Heavy Armored Dataholics Brow Plate (Delayed Secondary Parallax)
    int bx = cx + (int)(_browX * 16.0f);
    int by = cy - hh - 4 - (int)(isLeft ? _browLiftL : _browLiftR) + (int)(_browY * 6.0f);
    int bw = hw + 4;
    int dy = (int)(sin(browAng) * bw);

    int x0 = bx - bw;
    int y0 = by - dy + (dropL / 3);
    int x1 = bx + bw;
    int y1 = by + dy + (dropR / 3);

    // Thick 5px Dataholics Orange Armored Brow Blade
    for (int t = -2; t <= 2; t++) {
      _canvas.drawLine(x0, y0 + t, x1, y1 + t, armorCol);
    }
    // Outer wing flare on brow
    if (isLeft) {
      _canvas.fillTriangle(x0 - 4, y0 - 5, x0 + 8, y0 - 2, x0, y0 + 3, armorCol);
    } else {
      _canvas.fillTriangle(x1 + 4, y1 - 5, x1 - 8, y1 - 2, x1, y1 + 3, armorCol);
    }
  }
};
