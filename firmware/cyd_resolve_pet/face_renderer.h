#pragma once
#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============================================================================
// DigiVice Cyber-Face & Expressive Eyes Renderer (LovyanGFX 320x240 CYD)
// Memory-Safe 2.5D Depth Engine (208x124 Sprite Buffer + Direct HUD),
// Slanted Eyelids, Floating Eyebrows, 3D Pupils with Catchlights & Voice Wave
// ============================================================================

enum PetMood {
  MOOD_BOOT,
  MOOD_IDLE,
  MOOD_ALARM,       // Ticket Opened (Combat crimson, angry angled brows, contracted pupils)
  MOOD_LAUGH,       // Ticket Updated (Solar amber, happy crescent curves ^ ^)
  MOOD_CHOMP,       // Ticket Resolved (Emerald pulse, wide victory eyes)
  MOOD_POKED        // User touched screen (Flinch & glitch reaction)
};

enum IdleSubState {
  IDLE_REST,
  IDLE_WALK,
  IDLE_SNIFF,
  IDLE_PLAY_WISP,
  IDLE_DOZE,
  IDLE_SLEEP
};

struct TicketItem {
  int id;
  String title;
  String status;
  String priority;
  String requester;
  String email;
  String description;
};

// 16-Bit RGB565 Calibrated Colors
#define C_VOID_BG      0x0842 // Deep cyber dark (#070a10)
#define C_NEON_CYAN    0x3DFE // Primary cyan (#38bdf8)
#define C_CYAN_DIM     0x1290 // Dim cyan aura
#define C_ALERT_RED    0xF986 // Crimson alert (#f43f5e)
#define C_AMBER_GOLD   0xFCE0 // Warm amber (#f59e0b)
#define C_LIME_GREEN   0x2F4D // Slain victory lime (#2be86b)
#define C_PLASMA_PURP  0xAABF // Touch/glitch violet (#a855f7)
#define C_SLEEP_INDIGO 0x633F // Slumber indigo (#6366f1)
#define C_WHITE_GLOW   0xFFFF // Pure white highlight
#define C_GLASS_SHINE  0xCE59 // Soft specular reflection
#define C_FRAME_BORDER 0x18E3 // Subtle frame divider

class CyberFaceRenderer {
private:
  LGFX* _gfx;
  LGFX_Sprite _canvas;
  LGFX_Sprite _banner;
  bool _spriteReady;
  bool _bgCleared;

  static const int FACE_W = 208;
  static const int FACE_H = 128;
  static const int FACE_X = 56;  // (320 - 208) / 2
  static const int FACE_Y = 50;  // Centered between Top HUD (26) and Bottom Ticker (204)

  uint32_t _animTick;
  int _screenShakeX;
  int _screenShakeY;

  // Gaze physics (Lerp tracking)
  float _gazeX, _gazeY;
  float _targetGazeX, _targetGazeY;
  float _browX, _browY;
  float _blink;
  uint32_t _nextBlinkTick;

  // Eyelids & eyebrows
  float _upperLid;
  float _lowerLid;
  float _browAngle;
  float _browLift;
  float _pupilScale;

  // Touch tracking from hardware
  bool _touchActive;
  int _touchX, _touchY;

  IdleSubState _idleSubState;
  uint32_t _idleTimer;

public:
  CyberFaceRenderer(LGFX* gfx)
    : _gfx(gfx), _canvas(gfx), _banner(gfx), _spriteReady(false), _bgCleared(false),
      _animTick(0), _screenShakeX(0), _screenShakeY(0),
      _gazeX(0), _gazeY(0), _targetGazeX(0), _targetGazeY(0), _browX(0), _browY(0),
      _blink(0), _nextBlinkTick(75), _upperLid(0), _lowerLid(0), _browAngle(0), _browLift(0),
      _pupilScale(1.0f), _touchActive(false), _touchX(160), _touchY(120),
      _idleSubState(IDLE_REST), _idleTimer(0) {}

  void begin() {
    // Try 16-bit color depth first (208x128x2 = 53,248 bytes, fits comfortably in ESP32 DRAM)
    _canvas.setColorDepth(16);
    if (_canvas.createSprite(FACE_W, FACE_H) != nullptr) {
      _spriteReady = true;
      Serial.println("[FACE] 16-bit 208x128 Sprite allocated OK!");
    } else {
      // Fallback to 8-bit RGB332 (208x128x1 = 26,624 bytes) if heap is fragmented
      _canvas.setColorDepth(8);
      if (_canvas.createSprite(FACE_W, FACE_H) != nullptr) {
        _spriteReady = true;
        Serial.println("[FACE] 8-bit 208x128 Sprite allocated OK!");
      } else {
        _spriteReady = false;
        Serial.println("[FACE] WARN: Sprite alloc failed, using direct draw");
      }
    }

    _banner.setColorDepth(16);
    if (_banner.createSprite(320, 36) == nullptr) {
      _banner.setColorDepth(8);
      _banner.createSprite(320, 36);
    }

    _animTick = 0;
    _gazeX = 0;
    _gazeY = 0;
    _idleSubState = IDLE_REST;
    _idleTimer = 0;
    _bgCleared = false;
  }

  void setTouch(bool active, int x = 160, int y = 120) {
    _touchActive = active;
    if (active) {
      _touchX = x;
      _touchY = y;
      _targetGazeX = constrain((x - 160) / 100.0f, -1.0f, 1.0f);
      _targetGazeY = constrain((y - 115) / 70.0f, -1.0f, 1.0f);
    }
  }

  void setShake(int intensity) {
    if (intensity > 0) {
      _screenShakeX = random(-intensity, intensity);
      _screenShakeY = random(-intensity, intensity);
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

  void playBootAnimation(void (*audioCb)(int, int)) {
    _gfx->fillScreen(0x0000);

    for (int y = 0; y < 240; y += 8) {
      _gfx->drawFastHLine(0, y, 320, 0x10A2);
      delay(6);
    }

    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(2);
    _gfx->drawString("DIGIVICE CYBER-FACE", 36, 60);

    _gfx->setTextColor(0x9CF3);
    _gfx->setTextSize(1);
    _gfx->drawString("DATAHOLICS SOVEREIGN AI // 2.5D EXPRESSIVE ENGINE", 18, 94);

    _gfx->drawRoundRect(30, 124, 260, 14, 4, 0x2965);
    for (int w = 0; w <= 256; w += 16) {
      _gfx->fillRoundRect(32, 126, w, 10, 3, C_NEON_CYAN);
      if (audioCb && w % 32 == 0) audioCb(600 + w * 2, 18);
      delay(25);
    }

    if (audioCb) {
      audioCb(880, 50);
      audioCb(1174, 90);
    }
    delay(300);
    _gfx->fillScreen(C_VOID_BG);
    _bgCleared = false;
  }

  void clearScreen() {
    _gfx->fillScreen(C_VOID_BG);
    _bgCleared = false;
  }

  void drawConferenceBadge(const String &nameFirst, const String &nameLast, const String &role, const String &mac) {
    _bgCleared = false;
    _gfx->startWrite();
    _gfx->fillScreen(0x0821);

    _gfx->drawRect(8, 8, 304, 224, C_NEON_CYAN);
    _gfx->drawRect(10, 10, 300, 220, 0x18E3);

    _gfx->fillRect(10, 10, 300, 24, 0x10A2);
    _gfx->setTextColor(C_AMBER_GOLD);
    _gfx->setTextSize(1);
    _gfx->drawString("DATAHOLICS SOVEREIGN AI // VIP PASS", 28, 18);

    // Draw mini expressive eyes at top of badge
    if (_spriteReady) {
      _canvas.fillScreen(0x0821);
      _drawSingleEye(64, 34, 46, 34, true, C_NEON_CYAN, 0.35f);
      _drawSingleEye(144, 34, 46, 34, false, C_NEON_CYAN, 0.35f);
      _canvas.pushSprite(_gfx, FACE_X, 36);
    }

    _gfx->setTextSize(3);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(nameFirst, 40, 116);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString(nameLast, 40, 146);

    _gfx->fillRect(32, 178, 256, 22, 0x10A2);
    _gfx->drawRect(32, 178, 256, 22, C_NEON_CYAN);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->setTextSize(1);
    _gfx->drawString("CEO • CHIEF EXECUTIVE OFFICER", 56, 185);

    _gfx->setTextColor(0x9CF3);
    _gfx->drawString(String("SILICON ID: ") + mac + " • TAP TO RETURN", 24, 214);

    _gfx->endWrite();
  }

  void drawTicketListScreen(const TicketItem tickets[], int count) {
    _bgCleared = false;
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    _gfx->fillRect(0, 0, 320, 26, 0x10A2);
    _gfx->drawFastHLine(0, 26, 320, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString("< VOLVER", 10, 8);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString("SERVICEDESK INCIDENTS STREAM", 84, 8);

    for (int i = 0; i < 4; i++) {
      int y = 32 + i * 42;
      if (i < count) {
        _gfx->fillRoundRect(8, y, 304, 38, 4, 0x10A2);
        _gfx->drawRoundRect(8, y, 304, 38, 4, 0x2104);

        uint16_t stColor = (tickets[i].status == "resolved" || tickets[i].status == "RESOLVED") ? C_LIME_GREEN :
                           ((tickets[i].status == "in_progress" || tickets[i].status == "UPDATED") ? C_AMBER_GOLD : C_ALERT_RED);
        _gfx->fillRect(8, y, 5, 38, stColor);

        _gfx->setTextColor(stColor);
        _gfx->drawString(String("#") + tickets[i].id + " [" + tickets[i].status + "] PRIO: " + tickets[i].priority, 20, y + 6);

        _gfx->setTextColor(C_WHITE_GLOW);
        _gfx->drawString(tickets[i].title.substring(0, 42), 20, y + 21);
      } else {
        _gfx->fillRect(8, y, 304, 38, 0x0821);
        _gfx->drawRect(8, y, 304, 38, 0x18E3);
        _gfx->setTextColor(0x528A);
        _gfx->drawString("-- SIN INCIDENTES ADICIONALES --", 58, y + 14);
      }
    }

    _gfx->fillRect(0, 206, 320, 34, 0x10A2);
    _gfx->drawFastHLine(0, 206, 320, 0x2104);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("TOCA CUALQUIER TICKET PARA ABRIR EL DOSSIER", 22, 218);
    _gfx->endWrite();
  }

  void drawTicketDetailScreen(const TicketItem &ticket) {
    _bgCleared = false;
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    _gfx->fillRect(0, 0, 320, 26, 0x10A2);
    _gfx->drawFastHLine(0, 26, 320, C_NEON_CYAN);
    _gfx->setTextColor(C_AMBER_GOLD);
    _gfx->setTextSize(1);
    _gfx->drawString("< LISTA", 10, 8);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(String("INCIDENT #") + ticket.id, 110, 8);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("ROSTRO >", 254, 8);

    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(ticket.title.substring(0, 46), 14, 38);

    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString(String("Por: ") + ticket.requester + " | Prio: " + ticket.priority, 14, 56);

    _gfx->fillRoundRect(10, 72, 300, 120, 4, 0x10A2);
    _gfx->drawRoundRect(10, 72, 300, 120, 4, 0x2965);
    _gfx->setTextColor(C_GLASS_SHINE);

    String desc = ticket.description;
    if (desc.length() == 0) desc = "Incidente activo monitoreado por DigiVice Cyber-Face.";
    int lineY = 82;
    for (int p = 0; p < (int)desc.length() && lineY < 180; p += 44) {
      _gfx->drawString(desc.substring(p, min((int)desc.length(), p + 44)), 18, lineY);
      lineY += 14;
    }

    _gfx->fillRoundRect(10, 202, 142, 30, 4, 0x2104);
    _gfx->drawRoundRect(10, 202, 142, 30, 4, C_AMBER_GOLD);
    _gfx->setTextColor(C_AMBER_GOLD);
    _gfx->drawString("< LISTA TICKETS", 26, 213);

    _gfx->fillRoundRect(168, 202, 142, 30, 4, 0x10A2);
    _gfx->drawRoundRect(168, 202, 142, 30, 4, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("VOLVER AL ROSTRO", 184, 213);
    _gfx->endWrite();
  }

  void drawEnrollmentScreen(const String &siliconMac, const String &statusText) {
    _bgCleared = false;
    _gfx->startWrite();
    _gfx->fillScreen(0x0000);
    _gfx->drawRect(10, 10, 300, 220, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(2);
    _gfx->drawString("CYBER-FACE ENCLAVE", 34, 25);

    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->setTextSize(1);
    _gfx->drawString("FACTORY SILICON eFUSE MAC:", 34, 60);

    _gfx->fillRect(30, 75, 260, 30, 0x10A2);
    _gfx->drawRect(30, 75, 260, 30, C_ALERT_RED);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(2);
    _gfx->drawString(siliconMac, 42, 82);

    _gfx->setTextColor(0x9CF3);
    _gfx->setTextSize(1);
    _gfx->drawString("Estado: ", 34, 120);
    _gfx->drawString(statusText, 90, 120);

    _gfx->fillRect(30, 172, 260, 26, 0x10A2);
    _gfx->drawRect(30, 172, 260, 26, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("[ TOCAR PARA PORTAL WI-FI ]", 56, 180);

    _gfx->setTextColor(C_GLASS_SHINE);
    _gfx->drawString("Toca fuera para volver al rostro", 52, 208);
    _gfx->endWrite();
  }

  void renderFrame(PetMood mood, const String &ticketTitle, int ticketId, const String &ticketStatus, int activeCount, int resolvedCount, const String &deviceMac) {
    _animTick++;
    _idleTimer++;

    if (!_bgCleared) {
      _gfx->fillScreen(C_VOID_BG);
      _bgCleared = true;
    }

    // Color theme per mood
    uint16_t accent = C_NEON_CYAN;
    if (mood == MOOD_ALARM) accent = C_ALERT_RED;
    else if (mood == MOOD_LAUGH) accent = C_AMBER_GOLD;
    else if (mood == MOOD_CHOMP) accent = C_LIME_GREEN;
    else if (mood == MOOD_POKED) accent = C_PLASMA_PURP;

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
      tAng = 0.42f; // Angry V-shape (\ /)
      tLift = -5.0f;
      tPupil = 0.70f;
    } else if (mood == MOOD_LAUGH) {
      tLower = 0.52f; // Happy squint
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

    // 1. Render Expressive Face inside 208x128 Sprite Buffer
    if (_spriteReady) {
      _canvas.fillScreen(C_VOID_BG);

      // Subtle background grid lines inside visor
      for (int gy = 16; gy < FACE_H; gy += 24) {
        _canvas.drawFastHLine(8, gy, FACE_W - 16, 0x10A2);
      }

      // Draw Left Eye (center X=56, Y=58) and Right Eye (center X=152, Y=58)
      _drawSingleEye(56, 58, 68, 52, true, accent, 1.0f);
      _drawSingleEye(152, 58, 68, 52, false, accent, 1.0f);

      // Draw Holographic Vocal Waveform / Expressive Mouth at bottom of face
      int mx = (FACE_W / 2) + (int)(_gazeX * 8.0f);
      int my = 108 + (int)(_gazeY * 4.0f);

      if (mood == MOOD_LAUGH || mood == MOOD_CHOMP) {
        // Glowing happy smile arc
        _canvas.fillCircle(mx, my - 4, 14, accent);
        _canvas.fillCircle(mx, my - 7, 13, C_VOID_BG);
        _canvas.fillRect(mx - 16, my - 20, 32, 14, C_VOID_BG);
      } else if (mood == MOOD_ALARM) {
        // High-frequency alarm sawtooth wave
        int prevX = mx - 24;
        int prevY = my;
        for (int i = -20; i <= 24; i += 4) {
          int wy = my + (int)(sin((i * 0.7f) + (_animTick * 0.6f)) * 6.0f);
          _canvas.drawLine(prevX, prevY, mx + i, wy, accent);
          prevX = mx + i;
          prevY = wy;
        }
      } else {
        // Smooth living voice synthesizer wave
        int prevX = mx - 20;
        int prevY = my;
        for (int i = -18; i <= 20; i += 2) {
          float env = 1.0f - (abs(i) / 22.0f);
          int wy = my + (int)(sin((i * 0.4f) + (_animTick * 0.25f)) * 4.0f * env);
          _canvas.drawLine(prevX, prevY, mx + i, wy, accent);
          prevX = mx + i;
          prevY = wy;
        }
      }

      _canvas.pushSprite(_gfx, FACE_X + _screenShakeX, FACE_Y + _screenShakeY);
    }

    // 2. Render Side Visor Brackets & CPU/RAM Bars + Top HUD
    _gfx->startWrite();

    // Side Visor Corner Brackets (Left & Right of the 208x128 face sprite)
    _gfx->drawFastHLine(38, 42, 18, accent);
    _gfx->drawFastVLine(38, 42, 18, accent);
    _gfx->drawFastHLine(264, 42, 18, accent);
    _gfx->drawFastVLine(282, 42, 18, accent);

    _gfx->drawFastHLine(38, 184, 18, accent);
    _gfx->drawFastVLine(38, 166, 18, accent);
    _gfx->drawFastHLine(264, 184, 18, accent);
    _gfx->drawFastVLine(282, 166, 18, accent);

    // Left Telemetry Bar (CPU Pulse)
    _gfx->setTextColor(0x9CF3, C_VOID_BG);
    _gfx->setTextSize(1);
    _gfx->drawString("CPU", 8, 52);
    _gfx->drawRect(12, 66, 8, 92, 0x2965);
    int cpuFill = 28 + (int)(sin(_animTick * 0.08f) * 14.0f) + (mood == MOOD_ALARM ? 36 : 0);
    cpuFill = constrain(cpuFill, 8, 88);
    _gfx->fillRect(14, 68, 4, 88 - cpuFill, C_VOID_BG);
    _gfx->fillRect(14, 68 + (88 - cpuFill), 4, cpuFill, accent);

    // Right Telemetry Bar (RAM)
    _gfx->drawString("RAM", 294, 52);
    _gfx->drawRect(300, 66, 8, 92, 0x2965);
    int ramFill = 46;
    _gfx->fillRect(302, 68, 4, 88 - ramFill, C_VOID_BG);
    _gfx->fillRect(302, 68 + (88 - ramFill), 4, ramFill, C_NEON_CYAN);

    // Top HUD Bar (y = 0..24)
    _gfx->fillRect(0, 0, 320, 24, 0x10A2);
    _gfx->drawFastHLine(0, 24, 320, accent);

    // Left Touch Pill: Executive Badge
    _gfx->fillRoundRect(4, 3, 114, 18, 3, 0x2104);
    _gfx->drawRoundRect(4, 3, 114, 18, 3, accent);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString("ALONSO (CEO)", 16, 8);

    // Active Tickets Counter
    uint16_t actCol = (activeCount > 0) ? C_ALERT_RED : C_NEON_CYAN;
    _gfx->fillRoundRect(126, 3, 90, 18, 3, 0x18E3);
    _gfx->drawRoundRect(126, 3, 90, 18, 3, actCol);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(String("ACT: ") + activeCount, 136, 8);

    // Resolved Tickets Counter
    _gfx->fillRoundRect(222, 3, 94, 18, 3, 0x0280);
    _gfx->drawRoundRect(222, 3, 94, 18, 3, C_LIME_GREEN);
    _gfx->setTextColor(C_LIME_GREEN);
    _gfx->drawString(String("SLAIN: ") + resolvedCount, 230, 8);

    // 3. Bottom Ticker Banner (y = 204..240)
    _banner.fillScreen(0x0821);
    _banner.drawFastHLine(0, 0, 320, accent);
    _banner.fillCircle(12, 18, 4, accent);
    _banner.setTextColor(C_WHITE_GLOW);
    _banner.setTextSize(1);
    String tickerStr = String("[TKT-") + ticketId + "] " + ticketTitle;
    _banner.drawString(tickerStr.substring(0, 34), 22, 8);
    _banner.setTextColor(accent);
    _banner.drawString("TOCA PARA VER INCIDENTES [DECK >]", 22, 21);
    _banner.pushSprite(_gfx, 0, 204);

    _gfx->endWrite();
  }

private:
  void _drawSingleEye(int cx, int cy, int w, int h, bool isLeft, uint16_t accent, float parallaxMult) {
    int breathY = (int)(sin(_animTick * 0.08f) * 2.0f);
    int gx = (int)(_gazeX * 14.0f * parallaxMult);
    int gy = (int)(_gazeY * 9.0f * parallaxMult) + breathY;
    int x = cx + gx;
    int y = cy + gy;

    // 1. Outer Aura Ring (Depth halo)
    _canvas.drawRoundRect(x - w / 2 - 3, y - h / 2 - 3, w + 6, h + 6, 16, 0x18E3);
    _canvas.drawRoundRect(x - w / 2 - 1, y - h / 2 - 1, w + 2, h + 2, 15, C_CYAN_DIM);

    // 2. Main Glowing Eye Sclera/Visor
    _canvas.fillRoundRect(x - w / 2, y - h / 2, w, h, 14, accent);

    // 3. Subtle Horizontal Cyber Scanlines inside Eye
    for (int sy = y - h / 2 + 3; sy < y + h / 2 - 3; sy += 4) {
      _canvas.drawFastHLine(x - w / 2 + 4, sy, w - 8, 0x0842);
    }

    // 4. 3D Spherical Pupil & Iris (Moves with 1.4x extra parallax for depth!)
    int px = x + (int)(_gazeX * 8.0f * parallaxMult);
    int py = y + (int)(_gazeY * 6.0f * parallaxMult);
    int pRad = (int)(13.0f * _pupilScale);

    // Dark Void Iris Core
    _canvas.fillCircle(px, py, pRad, C_VOID_BG);
    // Glowing Inner Ring
    _canvas.drawCircle(px, py, (pRad * 2) / 3, C_WHITE_GLOW);

    // Primary Specular Catchlight (Top-Left 3D Glass Reflection)
    _canvas.fillCircle(px - pRad / 3 - 1, py - pRad / 3 - 1, 4, C_WHITE_GLOW);
    // Secondary Catchlight (Bottom-Right Reflection)
    _canvas.fillCircle(px + pRad / 3 + 1, py + pRad / 3 + 1, 2, C_GLASS_SHINE);

    // 5. Expressive Upper Eyelid Mask (with Slant for Angry/Sad/Curious angles)
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

    // 7. Floating Expressive Eyebrow Plate (Secondary Delayed Parallax)
    int bx = cx + (int)(_browX * 17.0f * parallaxMult);
    int by = cy - h / 2 - 10 - (int)_browLift + (int)(_browY * 7.0f * parallaxMult) + breathY;
    float ang = isLeft ? _browAngle : -_browAngle;
    int bw = (int)(w * 0.42f);
    int dy = (int)(sin(ang) * bw);

    // Thick 4px angled brow bar
    for (int t = -2; t <= 2; t++) {
      _canvas.drawLine(bx - bw, by - dy + t, bx + bw, by + dy + t, accent);
    }
  }
};
