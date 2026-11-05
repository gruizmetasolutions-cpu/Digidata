#pragma once
#include <Arduino.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// ============================================================================
// DigiVice Cyber-Face & Expressive Eyes Renderer (LovyanGFX 320x240 CYD)
// 2.5D Depth Engine, Smooth Eyelids, Floating Eyebrows, Pupils with Catchlights,
// Holographic Voice Waveform & Parallax Visor Cavity
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
#define C_VOID_BG      0x0842 // Deep cyber dark
#define C_NEON_CYAN    0x3DFE // Primary cyan (#38bdf8)
#define C_ALERT_RED    0xF986 // Crimson alert (#f43f5e)
#define C_AMBER_GOLD   0xFCE0 // Warm amber (#f59e0b)
#define C_LIME_GREEN   0x2F4D // Slain victory lime (#2be86b)
#define C_PLASMA_PURP  0xAABF // Scan mode violet (#a855f7)
#define C_SLEEP_INDIGO 0x633F // Slumber indigo (#6366f1)
#define C_WHITE_GLOW   0xFFFF // Pure white highlight
#define C_GLASS_SHINE  0xCE59 // Soft specular reflection
#define C_FRAME_BORDER 0x18E3 // Subtle frame divider

class CyberFaceRenderer {
private:
  LGFX* _gfx;
  LGFX_Sprite _canvas;
  LGFX_Sprite _banner;

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
  float _happyArc;
  int _glitchOffset;

  // Touch tracking from hardware
  bool _touchActive;
  int _touchX, _touchY;

  IdleSubState _idleSubState;
  uint32_t _idleTimer;

public:
  CyberFaceRenderer(LGFX* gfx)
    : _gfx(gfx), _canvas(gfx), _banner(gfx), _animTick(0), _screenShakeX(0), _screenShakeY(0),
      _gazeX(0), _gazeY(0), _targetGazeX(0), _targetGazeY(0), _browX(0), _browY(0),
      _blink(0), _nextBlinkTick(90), _upperLid(0), _lowerLid(0), _browAngle(0), _browLift(0),
      _pupilScale(1.0f), _happyArc(0), _glitchOffset(0), _touchActive(false), _touchX(160), _touchY(120),
      _idleSubState(IDLE_REST), _idleTimer(0) {}

  void begin() {
    _canvas.setColorDepth(16);
    _canvas.createSprite(320, 178);

    _banner.setColorDepth(16);
    _banner.createSprite(320, 36);

    _animTick = 0;
    _gazeX = 0;
    _gazeY = 0;
    _idleSubState = IDLE_REST;
    _idleTimer = 0;
  }

  void setTouch(bool active, int x = 160, int y = 120) {
    _touchActive = active;
    if (active) {
      _touchX = x;
      _touchY = y;
      _targetGazeX = constrain((x - 160) / 100.0f, -1.0f, 1.0f);
      _targetGazeY = constrain((y - 115) / 75.0f, -1.0f, 1.0f);
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

    // Booting cyber scanlines
    for (int y = 0; y < 240; y += 8) {
      _gfx->drawFastHLine(0, y, 320, 0x18E3);
      delay(8);
    }

    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(2);
    _gfx->drawString("DIGIVICE CYBER-FACE", 36, 68);

    _gfx->setTextColor(0x9CF3);
    _gfx->setTextSize(1);
    _gfx->drawString("DATAHOLICS SOVEREIGN AI // 2.5D EXPRESSIVE ENGINE", 22, 102);

    _gfx->drawRoundRect(30, 130, 260, 14, 4, 0x2965);
    for (int w = 0; w <= 256; w += 16) {
      _gfx->fillRoundRect(32, 132, w, 10, 3, C_NEON_CYAN);
      if (audioCb && w % 32 == 0) audioCb(600 + w * 2, 20);
      delay(35);
    }

    if (audioCb) {
      audioCb(880, 50);
      audioCb(1174, 90);
    }
    delay(400);
    _gfx->fillScreen(C_VOID_BG);
  }

  void clearScreen() {
    _gfx->fillScreen(C_VOID_BG);
  }

  void drawConferenceBadge(const String &nameFirst, const String &nameLast, const String &role, const String &mac) {
    _gfx->startWrite();
    _gfx->fillScreen(0x0609);

    // Cyber border
    _gfx->drawRect(8, 8, 304, 224, C_NEON_CYAN);
    _gfx->drawRect(10, 10, 300, 220, 0x18E3);

    // Header
    _gfx->fillRect(10, 10, 300, 24, 0x10A2);
    _gfx->setTextColor(C_AMBER_GOLD);
    _gfx->setTextSize(1);
    _gfx->drawString("⚡ DATAHOLICS SOVEREIGN AI // VIP PASS", 22, 16);

    // Mini animated eyes inside the badge!
    int badgeEyeY = 72;
    _drawEyeShape(&_canvas, 115, badgeEyeY, 44, 32, true, C_NEON_CYAN, 0.4f);
    _drawEyeShape(&_canvas, 205, badgeEyeY, 44, 32, false, C_NEON_CYAN, 0.4f);
    _canvas.pushSprite(_gfx, 0, 34);

    _gfx->setTextSize(3);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(nameFirst, 36, 122);
    _gfx->drawString(nameLast, 36, 150);

    _gfx->fillRect(32, 180, 256, 22, 0x10A2);
    _gfx->drawRect(32, 180, 256, 22, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString("CHIEF EXECUTIVE OFFICER (CEO)", 48, 186);

    _gfx->setTextColor(0x633F);
    _gfx->drawString(String("SILICON eFUSE: ") + mac + " • [TAP TO RETURN]", 24, 214);

    _gfx->endWrite();
  }

  void drawTicketListScreen(const TicketItem tickets[], int count) {
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    // Header
    _gfx->fillRect(0, 0, 320, 26, 0x10A2);
    _gfx->drawFastHLine(0, 26, 320, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString("< VOLVER", 10, 8);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString("SERVICEDESK INCIDENTS STREAM", 84, 8);

    for (int i = 0; i < count && i < 4; i++) {
      int y = 32 + i * 46;
      _gfx->fillRoundRect(8, y, 304, 40, 4, 0x10A2);
      _gfx->drawRoundRect(8, y, 304, 40, 4, 0x2104);

      uint16_t stColor = (tickets[i].status == "resolved" || tickets[i].status == "RESOLVED") ? C_LIME_GREEN :
                         ((tickets[i].status == "updated" || tickets[i].status == "UPDATED") ? C_AMBER_GOLD : C_ALERT_RED);
      _gfx->fillRect(8, y, 5, 40, stColor);

      _gfx->setTextColor(C_WHITE_GLOW);
      _gfx->drawString(String("#") + tickets[i].id + " " + tickets[i].title.substring(0, 24), 20, y + 8);

      _gfx->setTextColor(0x9CF3);
      _gfx->drawString(String("STATUS: ") + tickets[i].status + " | PRIO: " + tickets[i].priority, 20, y + 23);
    }
    _gfx->endWrite();
  }

  void drawTicketDetailScreen(const TicketItem &ticket) {
    _gfx->startWrite();
    _gfx->fillScreen(C_VOID_BG);

    _gfx->fillRect(0, 0, 320, 26, 0x10A2);
    _gfx->drawFastHLine(0, 26, 320, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->setTextSize(1);
    _gfx->drawString(String("INCIDENT DOSSIER #") + ticket.id, 12, 8);

    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString(ticket.title, 14, 40);

    _gfx->setTextColor(0x3DFE);
    _gfx->drawString(String("Solicitante: ") + ticket.requester + " | " + ticket.priority, 14, 58);

    _gfx->fillRoundRect(10, 74, 300, 114, 4, 0x10A2);
    _gfx->drawRoundRect(10, 74, 300, 114, 4, 0x2965);
    _gfx->setTextColor(C_GLASS_SHINE);
    _gfx->drawString(ticket.description.substring(0, 140), 18, 86);

    _gfx->fillRoundRect(10, 200, 142, 28, 4, 0x2104);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->drawString("< LISTA TICKETS", 24, 208);

    _gfx->fillRoundRect(168, 200, 142, 28, 4, 0x18E3);
    _gfx->drawRoundRect(168, 200, 142, 28, 4, C_NEON_CYAN);
    _gfx->setTextColor(C_NEON_CYAN);
    _gfx->drawString("VOLVER AL ROSTRO 👁️", 176, 208);
    _gfx->endWrite();
  }

  void drawEnrollmentScreen(const String &siliconMac, const String &statusText) {
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
    _gfx->drawString("[ 📡 TOCAR PARA PORTAL WI-FI ]", 44, 180);

    _gfx->setTextColor(C_GLASS_SHINE);
    _gfx->drawString("Toca fuera para volver al rostro", 52, 208);
    _gfx->endWrite();
  }

  void renderFrame(PetMood mood, const String &ticketTitle, int ticketId, const String &ticketStatus, int activeCount, int resolvedCount, const String &deviceMac) {
    _animTick++;
    _idleTimer++;

    // Color theme per mood
    uint16_t accent = C_NEON_CYAN;
    if (mood == MOOD_ALARM) accent = C_ALERT_RED;
    else if (mood == MOOD_LAUGH) accent = C_AMBER_GOLD;
    else if (mood == MOOD_CHOMP) accent = C_LIME_GREEN;
    else if (mood == MOOD_POKED) accent = C_PLASMA_PURP;

    // Autonomous Saccades & Blinking
    if (!_touchActive) {
      if (_animTick % 70 == 0) {
        _targetGazeX = (random(-10, 11) / 10.0f) * 0.85f;
        _targetGazeY = (random(-10, 11) / 10.0f) * 0.55f;
      } else if (_animTick % 180 == 0) {
        _targetGazeX = 0;
        _targetGazeY = 0;
      }
    }

    if (_animTick >= _nextBlinkTick) {
      _blink = 1.0f;
      _nextBlinkTick = _animTick + 100 + random(0, 120);
    }
    _blink += (0.0f - _blink) * 0.35f;

    _gazeX += (_targetGazeX - _gazeX) * 0.22f;
    _gazeY += (_targetGazeY - _gazeY) * 0.22f;
    _browX += (_gazeX - _browX) * 0.12f;
    _browY += (_gazeY - _browY) * 0.12f;

    // Mood target variables
    float tUpper = 0.05f;
    float tLower = 0.0f;
    float tAng = 0.0f;
    float tLift = 0.0f;
    float tPupil = 1.0f;

    if (mood == MOOD_ALARM) {
      tUpper = 0.30f;
      tAng = 0.40f; // V-shape angry
      tLift = -5.0f;
      tPupil = 0.72f;
    } else if (mood == MOOD_LAUGH) {
      tLower = 0.50f;
      tAng = -0.22f;
      tLift = 6.0f;
    } else if (mood == MOOD_CHOMP) {
      tUpper = 0.0f;
      tLift = 8.0f;
      tPupil = 1.25f;
    } else if (mood == MOOD_POKED) {
      tPupil = 0.65f;
      tLift = 9.0f;
    }

    _upperLid += (max(tUpper, _blink) - _upperLid) * 0.25f;
    _lowerLid += (tLower - _lowerLid) * 0.22f;
    _browAngle += (tAng - _browAngle) * 0.20f;
    _browLift += (tLift - _browLift) * 0.20f;
    _pupilScale += (tPupil - _pupilScale) * 0.20f;

    // 1. Render Face Canvas (320x178)
    _canvas.fillScreen(C_VOID_BG);

    // Visor Parallax Brackets
    int vx = 160 - (int)(_gazeX * 6.0f);
    int vy = 89 - (int)(_gazeY * 4.0f);
    _canvas.drawRoundRect(vx - 110, vy - 65, 220, 130, 20, 0x18E3);
    _canvas.drawFastHLine(vx - 105, vy - 55, 14, accent);
    _canvas.drawFastVLine(vx - 105, vy - 55, 14, accent);
    _canvas.drawFastHLine(vx + 91, vy - 55, 14, accent);
    _canvas.drawFastVLine(vx + 105, vy - 55, 14, accent);

    // Side Telemetry Bars (CPU/RAM meters inside visor)
    _canvas.fillRect(10, 36, 5, 80, 0x10A2);
    int cpuH = 25 + (_animTick % 30);
    _canvas.fillRect(10, 116 - cpuH, 5, cpuH, accent);
    _canvas.setTextColor(0x633F);
    _canvas.setTextSize(1);
    _canvas.drawString("CPU", 8, 26);

    _canvas.fillRect(305, 36, 5, 80, 0x10A2);
    _canvas.fillRect(305, 76, 5, 40, C_NEON_CYAN);
    _canvas.drawString("RAM", 298, 26);

    // Left and Right Expressive Eyes
    _drawEyeShape(&_canvas, 112, 85, 68, 54, true, accent, 1.0f);
    _drawEyeShape(&_canvas, 208, 85, 68, 54, false, accent, 1.0f);

    // Holographic Voice Waveform / Expressive Mouth
    int mx = 160 + (int)(_gazeX * 8.0f);
    int my = 142 + (int)(_gazeY * 4.0f);
    if (mood == MOOD_LAUGH || mood == MOOD_CHOMP) {
      _canvas.drawCircle(mx, my - 6, 14, accent);
      _canvas.fillRect(mx - 16, my - 22, 32, 16, C_VOID_BG);
    } else {
      for (int i = -16; i <= 16; i += 2) {
        int wy = my + (int)(sin((i * 0.45f) + (_animTick * 0.25f)) * 3.0f);
        _canvas.drawPixel(mx + i, wy, accent);
      }
    }

    _canvas.pushSprite(_gfx, 0, 26);

    // 2. Render Top HUD Banner (on screen directly)
    _gfx->startWrite();
    _gfx->fillRect(0, 0, 320, 26, 0x0821);
    _gfx->drawFastHLine(0, 25, 320, 0x18E3);

    // Executive Badge Pill
    _gfx->fillRoundRect(4, 3, 110, 19, 4, 0x10A2);
    _gfx->drawRoundRect(4, 3, 110, 19, 4, accent);
    _gfx->setTextColor(C_WHITE_GLOW);
    _gfx->setTextSize(1);
    _gfx->drawString("🪪 ALONSO (CEO)", 10, 8);

    // Metric Pills
    uint16_t actCol = (activeCount > 0) ? C_ALERT_RED : 0x633F;
    _gfx->fillRoundRect(184, 3, 62, 19, 4, 0x10A2);
    _gfx->drawRoundRect(184, 3, 62, 19, 4, actCol);
    _gfx->setTextColor(actCol);
    _gfx->drawString(String("ACT: ") + activeCount, 194, 8);

    _gfx->fillRoundRect(252, 3, 64, 19, 4, 0x10A2);
    _gfx->drawRoundRect(252, 3, 64, 19, 4, C_LIME_GREEN);
    _gfx->setTextColor(C_LIME_GREEN);
    _gfx->drawString(String("OK: ") + resolvedCount, 262, 8);
    _gfx->endWrite();

    // 3. Render Bottom Ticker Banner (320x36)
    _banner.fillScreen(0x0821);
    _banner.drawFastHLine(0, 0, 320, accent);
    _banner.fillCircle(12, 18, 4, accent);
    _banner.setTextColor(C_WHITE_GLOW);
    _banner.setTextSize(1);
    String tickerText = String("INCIDENT: #") + ticketId + " " + ticketTitle;
    _banner.drawString(tickerText.substring(0, 32), 22, 13);
    _banner.setTextColor(accent);
    _banner.drawString("[DECK >]", 270, 13);
    _banner.pushSprite(_gfx, 0, 204);
  }

private:
  void _drawEyeShape(LGFX_Sprite* spr, int cx, int cy, int w, int h, bool isLeft, uint16_t accent, float parallaxMult) {
    int gx = (int)(_gazeX * 14.0f * parallaxMult);
    int gy = (int)(_gazeY * 9.0f * parallaxMult);
    int x = cx + gx;
    int y = cy + gy;

    // Outer glow ring
    spr->drawRoundRect(x - w / 2 - 2, y - h / 2 - 2, w + 4, h + 4, 18, 0x10A2);

    // Eye base body
    spr->fillRoundRect(x - w / 2, y - h / 2, w, h, 16, accent);

    // Inner horizontal scanlines for cyber depth
    for (int sy = y - h / 2 + 2; sy < y + h / 2 - 2; sy += 3) {
      spr->drawFastHLine(x - w / 2 + 3, sy, w - 6, 0x0000);
    }

    // 3D Inner Pupil & Catchlights
    int px = x + (int)(_gazeX * 7.0f * parallaxMult);
    int py = y + (int)(_gazeY * 5.0f * parallaxMult);
    int pRad = (int)(12.0f * _pupilScale);
    spr->fillCircle(px, py, pRad, 0x0000);
    spr->drawCircle(px, py, pRad / 2, 0xFFFF);

    // Specular Catchlights (Top-Left 3D Reflection)
    spr->fillCircle(px - 4, py - 4, 3, C_WHITE_GLOW);
    spr->fillCircle(px + 4, py + 4, 1, C_GLASS_SHINE);

    // Eyelid Closure
    if (_upperLid > 0.05f) {
      int lidH = (int)(_upperLid * (float)h);
      spr->fillRect(x - w / 2 - 2, y - h / 2 - 2, w + 4, lidH, C_VOID_BG);
    }
    if (_lowerLid > 0.05f) {
      int lidH = (int)(_lowerLid * (float)h);
      spr->fillRect(x - w / 2 - 2, y + h / 2 - lidH, w + 4, lidH, C_VOID_BG);
    }

    // Floating Eyebrow Plate (Delayed parallax)
    int bx = cx + (int)(_browX * 18.0f * parallaxMult);
    int by = cy - h / 2 - 10 - (int)_browLift + (int)(_browY * 8.0f * parallaxMult);
    float ang = isLeft ? _browAngle : -_browAngle;

    int browW = (int)(w * 0.8f);
    int yOffset = (int)(ang * 12.0f);
    spr->drawLine(bx - browW / 2, by - yOffset, bx + browW / 2, by + yOffset, accent);
    spr->drawLine(bx - browW / 2, by - yOffset - 1, bx + browW / 2, by + yOffset - 1, accent);
    spr->drawLine(bx - browW / 2, by - yOffset - 2, bx + browW / 2, by + yOffset - 2, accent);
  }
};
