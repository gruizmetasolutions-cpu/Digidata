#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "cyd_display.h"
#include "audio_synth.h"
#include "auth_crypto.h"
#include "wifi_manager.h"

// ============================================================================
// Display Mode Configuration
// Set USE_CYBER_FACE_RENDERER to 1 for the Expressive Animated Cyber-Face
// Set USE_CYBER_FACE_RENDERER to 0 for the classic Scrap-Hound Yokai Gremlin
// ============================================================================
#define USE_CYBER_FACE_RENDERER 1

#if USE_CYBER_FACE_RENDERER
#include "face_renderer.h"
#else
#include "pet_renderer.h"
#endif

// ============================================================================
// Dataholics Resolve ServiceDesk - DigiVice
// Cute Horror / Cyber-Face Edition (Hardware Cryptographic Silicon Binding)
// Target Board: ESP32-2432S028 (CYD 2.8" ST7789 320x240)
// ============================================================================

// Onboard Hardware Pinout (ESP32-2432S028 CYD)
#define RGB_LED_R       4    // Active LOW
#define RGB_LED_G      16    // Active LOW
#define RGB_LED_B      17    // Active LOW
#define LDR_PIN        34    // Ambient Light Sensor
#define BOOT_BTN_PIN    0    // Physical BOOT Button on back (Active LOW)

// Resolve ServiceDesk Secure Feed URL
const char* RESOLVE_FEED_URL = "https://resolve.dataholics.com.mx/digipet_feed.php";

// Pre-Shared Hardware Cryptographic Secret Key
// (Matched in Resolve DB `digipet_hardware` table via scratch/enroll_digipet.py)
String DEVICE_SECRET_KEY = "757618edd61e1f81993949992d11b46eb10d62db902c5e8d674229f52e48cc85";

// Hardware & Peripheral Instances
LGFX gfx;
ScrapHoundAudio soundFx;
#if USE_CYBER_FACE_RENDERER
CyberFaceRenderer petRenderer(&gfx);
#else
ScrapHoundRenderer petRenderer(&gfx);
#endif
DigiPetWiFiManager wifiManager;

// State Tracking
String siliconMac = "";
PetMood currentMood = MOOD_BOOT;
unsigned long moodExpiryMs = 0;

int prevTicketId = 0;
int prevCommentId = 0;
int prevUpdatedTs = 0;
int prevResolvedTs = 0;
String prevStatus = "";

int activeTicketCount = 0;
int resolvedTodayCount = 0;
String latestTicketTitle = "Awaiting Uplink...";
int latestTicketId = 0;
String latestTicketStatus = "idle";

// Interactive Screen Modes
enum ScreenMode {
  SCREEN_PET,           // Watchface 1: Full-Screen 2.5D Expressive Cyber-Face
  SCREEN_WATCH,         // Watchface 2: Smartwatch Chronograph & Complications
  SCREEN_BADGE,         // Alonso Ruiz CEO Conference Badge
  SCREEN_TICKET_LIST,   // ServiceDesk Recent Incidents Stream
  SCREEN_TICKET_DETAIL, // Deep Ticket Forensic Dossier
  SCREEN_ENROLLMENT     // Hardware Silicon MAC & Wi-Fi Enclave
};

ScreenMode currentScreen = SCREEN_PET;
int selectedTicketIdx = 0;
TicketItem cachedTickets[4];
int cachedTicketCount = 0;
unsigned long lastScreenInputMs = 0;

// Timing & Modes
unsigned long lastPollMs = 0;
const unsigned long POLL_INTERVAL_MS = 3500;
unsigned long touchStartTime = 0;
bool prevBootBtn = HIGH;
unsigned long bootBtnPressTime = 0;

void audioPlayToneWrapper(int freq, int durationMs) {
  soundFx.playTone(freq, durationMs);
}

// RGB Underglow Control (Active LOW PWM: 0 = max brightness, 255 = OFF)
void setRgbLed(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(RGB_LED_R, 255 - r);
  analogWrite(RGB_LED_G, 255 - g);
  analogWrite(RGB_LED_B, 255 - b);
}

// Always-On Living Candle-Flame Underglow (Red <-> Orange <-> Golden Yellow)
void updateUnderglow() {
  unsigned long ms = millis();
  float t = ms * 0.001f;

  // Multi-harmonic candle flame oscillation + organic micro-gust flicker
  float wave = (sin(t * 3.7f) * 0.35f) +
               (sin(t * 8.3f) * 0.25f) +
               (cos(t * 17.1f) * 0.20f) +
               ((random(0, 100) / 100.0f) * 0.20f);
  float norm = constrain((wave + 0.60f) / 1.40f, 0.0f, 1.0f);

  // Red stays hot and dominant (210..255); Green modulates (8..180) to shift Red -> Orange -> Golden Yellow
  uint8_t flameR = (uint8_t)constrain(215 + (int)(norm * 40.0f), 190, 255);
  uint8_t flameG = (uint8_t)constrain(8 + (int)(norm * 172.0f), 5, 185);

  if (currentMood == MOOD_ALARM) {
    // Rapid intense Red/Yellow fire flare on new ticket alarm
    if ((ms / 90) % 2 == 0) setRgbLed(255, 0, 0);
    else setRgbLed(255, 190, 0);
  } else {
    // Continuous living candle flame between Red and Golden Yellow
    setRgbLed(flameR, flameG, 0);
  }
}

// Secure Hardware-Bound Polling
void pollResolveFeed() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(2);

  HTTPClient http;
  http.setTimeout(2000);
  http.setReuse(false);
  if (!http.begin(client, RESOLVE_FEED_URL)) {
    Serial.println("[HTTP] Begin failed");
    return;
  }

  time_t now = time(nullptr);
  String ts = String((unsigned long)now);
  String nonce = HardwareAuth::generateNonce();
  String payload = "DIGIPET|" + siliconMac + "|" + ts + "|" + nonce;
  String signature = HardwareAuth::computeHmacSha256(DEVICE_SECRET_KEY, payload);

  http.addHeader("User-Agent", "ESP32-CyberWatch/3.5");
  http.addHeader("X-Device-Mac", siliconMac);
  http.addHeader("X-Timestamp", ts);
  http.addHeader("X-Nonce", nonce);
  http.addHeader("X-Signature", signature);

  int httpCode = http.GET();
  if (httpCode == HTTP_CODE_OK) {
    String resp = http.getString();
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, resp);

    if (!err) {
      int tktId = doc["latest_ticket"]["id"];
      const char* title = doc["latest_ticket"]["title"];
      const char* status = doc["latest_ticket"]["status"];
      int updTs = doc["latest_ticket"]["updated_ts"];
      int resTs = doc["latest_ticket"]["resolved_ts"];
      int cmtId = doc["latest_comment"]["id"];

      activeTicketCount = doc["metrics"]["active"];
      resolvedTodayCount = doc["metrics"]["resolved"];
      latestTicketId = tktId;
      latestTicketTitle = String(title);
      latestTicketStatus = String(status);

      if (prevTicketId > 0 && tktId > prevTicketId) {
        currentMood = MOOD_ALARM;
        moodExpiryMs = millis() + 9000;
        soundFx.playWarKlaxon();
      } else if (String(status) == "resolved" && (prevStatus != "resolved" || resTs > prevResolvedTs)) {
        if (prevTicketId > 0) {
          currentMood = MOOD_CHOMP;
          moodExpiryMs = millis() + 8000;
          petRenderer.setShake(6);
          soundFx.playEvilChompFanfare();
        }
      } else if (prevCommentId > 0 && (cmtId > prevCommentId || (updTs > prevUpdatedTs && String(status) != "resolved"))) {
        currentMood = MOOD_LAUGH;
        moodExpiryMs = millis() + 5000;
        soundFx.playManiacalLaugh();
      }

      prevTicketId = tktId;
      prevCommentId = cmtId;
      prevUpdatedTs = updTs;
      prevResolvedTs = resTs;
      prevStatus = String(status);

      JsonArray recArr = doc["recent_tickets"];
      if (recArr) {
        cachedTicketCount = 0;
        for (JsonObject item : recArr) {
          if (cachedTicketCount >= 4) break;
          cachedTickets[cachedTicketCount].id = item["id"] | 0;
          cachedTickets[cachedTicketCount].title = item["title"] | "No Title";
          cachedTickets[cachedTicketCount].status = item["status"] | "open";
          cachedTickets[cachedTicketCount].priority = item["priority"] | "normal";
          cachedTickets[cachedTicketCount].requester = item["requester_name"] | "User";
          cachedTickets[cachedTicketCount].email = item["requester_email"] | "";
          cachedTickets[cachedTicketCount].description = item["description"] | "";
          cachedTicketCount++;
        }
      }
    }
  } else {
    Serial.printf("[HTTP] Error code: %d\n", httpCode);
  }
  http.end();
}

void setup() {
  Serial.begin(115200);

  pinMode(RGB_LED_R, OUTPUT);
  pinMode(RGB_LED_G, OUTPUT);
  pinMode(RGB_LED_B, OUTPUT);
  setRgbLed(0, 0, 0);

  pinMode(LDR_PIN, INPUT);
  pinMode(BOOT_BTN_PIN, INPUT_PULLUP);

  gfx.begin();
  gfx.setRotation(1); // 320x240 Landscape
  gfx.setBrightness(245);

  soundFx.begin();
  petRenderer.begin();

  siliconMac = HardwareAuth::getSiliconMac();
  Serial.println("==========================================");
  Serial.println(" DATAHOLICS SMARTWATCH OS // CYBER-FACE");
  Serial.printf (" SILICON MAC: %s\n", siliconMac.c_str());
  Serial.println("==========================================");

  // Play 2.5s Executive Dataholics Boot Animation (Zero Flicker)
  petRenderer.playBootAnimation(audioPlayToneWrapper);

  // Initialize Wi-Fi Network Manager
  wifiManager.begin();

  // If BOOT button is held down on power-on, launch Wi-Fi Config Portal
  if (digitalRead(BOOT_BTN_PIN) == LOW) {
    Serial.println("[BOOT] Forced Wi-Fi Config Portal requested on power-on.");
    soundFx.playTap();
    wifiManager.startConfigPortal(&gfx, &soundFx, setRgbLed, 180);
  }

  // Fast non-blocking connection attempt (3s max) so boot goes straight into Smartwatch/Face
  bool connected = wifiManager.connectStored(&gfx, setRgbLed, 3);
  if (connected) {
    Serial.println("[WIFI] Connected! Syncing NTP...");
    setRgbLed(0, 255, 0);
    HardwareAuth::syncNtp();
    currentMood = MOOD_IDLE;
  } else {
    Serial.println("[WIFI] Offline/Local Smartwatch mode active.");
    currentMood = MOOD_IDLE;
  }

  soundFx.playTap();
  petRenderer.clearScreen();
  pollResolveFeed();
}

void cycleMoodInteractive() {
  if (currentMood == MOOD_IDLE) {
    currentMood = MOOD_LAUGH;
    soundFx.playManiacalLaugh();
  } else if (currentMood == MOOD_LAUGH) {
    currentMood = MOOD_ALARM;
    soundFx.playWarKlaxon();
  } else if (currentMood == MOOD_ALARM) {
    currentMood = MOOD_CHOMP;
    soundFx.playEvilChompFanfare();
  } else if (currentMood == MOOD_CHOMP) {
    currentMood = MOOD_POKED;
    soundFx.playGrowl();
  } else {
    currentMood = MOOD_IDLE;
    soundFx.playTap();
  }
  moodExpiryMs = (currentMood == MOOD_IDLE) ? 0 : (millis() + 8000);
}

void loop() {
  unsigned long now = millis();

  // 0. Physical BOOT Button: Short Press cycles Watchfaces/Badge | Long Press (>= 2.5s) = Wi-Fi Portal
  bool curBootBtn = digitalRead(BOOT_BTN_PIN);
  if (curBootBtn == LOW && prevBootBtn == HIGH) {
    bootBtnPressTime = now;
  } else if (curBootBtn == HIGH && prevBootBtn == LOW && now > lastScreenInputMs) {
    unsigned long holdMs = now - bootBtnPressTime;
    lastScreenInputMs = now + 400;
    if (holdMs >= 2500) {
      Serial.println("[BOOT] Long press: Launching Wi-Fi Config Portal...");
      soundFx.playTap();
      wifiManager.startConfigPortal(&gfx, &soundFx, setRgbLed, 180);
      currentScreen = SCREEN_PET;
      petRenderer.clearScreen();
    } else {
      if (currentScreen == SCREEN_PET) currentScreen = SCREEN_WATCH;
      else if (currentScreen == SCREEN_WATCH) currentScreen = SCREEN_BADGE;
      else currentScreen = SCREEN_PET;
      petRenderer.clearScreen();
      soundFx.playTap();
    }
  }
  prevBootBtn = curBootBtn;

  // Poll Resolve ServiceDesk (background polling)
  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    pollResolveFeed();
  }

  // 1. SCREEN_BADGE Mode (Alonso Ruiz / CEO)
  if (currentScreen == SCREEN_BADGE) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 400;
      currentScreen = SCREEN_PET;
      petRenderer.clearScreen();
      soundFx.playTap();
      return;
    }
    petRenderer.drawConferenceBadge("ALONSO", "RUIZ", "CEO", siliconMac);
    bool ledStep = (now / 400) % 2;
    if (ledStep) setRgbLed(255, 120, 0);
    else setRgbLed(0, 180, 255);
    delay(40);
    return;
  }

  // 2. SCREEN_TICKET_LIST Mode (Recent Incidents Stream)
  if (currentScreen == SCREEN_TICKET_LIST) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 350;
      if ((touchY < 30 && touchX > 210) || touchY >= 200) {
        currentScreen = SCREEN_PET;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      for (int i = 0; i < 4; i++) {
        int rowY = 32 + (i * 42);
        if (touchY >= rowY && touchY < (rowY + 40) && i < cachedTicketCount) {
          selectedTicketIdx = i;
          currentScreen = SCREEN_TICKET_DETAIL;
          petRenderer.clearScreen();
          soundFx.playTap();
          return;
        }
      }
    }
    petRenderer.drawTicketListScreen(cachedTickets, cachedTicketCount);
    setRgbLed(0, 120, 255);
    delay(50);
    return;
  }

  // 3. SCREEN_TICKET_DETAIL Mode (Deep Ticket Dossier)
  if (currentScreen == SCREEN_TICKET_DETAIL) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 350;
      if ((touchY < 30 && touchX < 100) || (touchY >= 200 && touchX < 160)) {
        currentScreen = SCREEN_TICKET_LIST;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      if ((touchY < 30 && touchX > 210) || (touchY >= 200 && touchX >= 160)) {
        currentScreen = SCREEN_WATCH;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
    }
    if (selectedTicketIdx < cachedTicketCount) {
      petRenderer.drawTicketDetailScreen(cachedTickets[selectedTicketIdx]);
    } else {
      currentScreen = SCREEN_TICKET_LIST;
    }
    setRgbLed(255, 120, 0);
    delay(50);
    return;
  }

  // 4. SCREEN_ENROLLMENT Mode (Hardware Silicon & Wi-Fi Enclave)
  if (currentScreen == SCREEN_ENROLLMENT) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 400;
      if (touchY >= 160 && touchY <= 204 && touchX >= 25 && touchX <= 295) {
        Serial.println("[TOUCH] Wi-Fi Portal requested from Enrollment Enclave.");
        soundFx.playTap();
        bool saved = wifiManager.startConfigPortal(&gfx, &soundFx, setRgbLed, 180);
        if (saved) {
          if (wifiManager.connectStored(&gfx, setRgbLed, 8)) {
            HardwareAuth::syncNtp();
          }
        }
        currentScreen = SCREEN_WATCH;
        petRenderer.clearScreen();
        return;
      }
      currentScreen = SCREEN_WATCH;
      petRenderer.clearScreen();
      soundFx.playTap();
      return;
    }
    petRenderer.drawEnrollmentScreen(siliconMac, (WiFi.status() == WL_CONNECTED) ? "ONLINE" : "LOCAL / OFFLINE");
    delay(60);
    return;
  }

  // Reset temporary mood after expiry
  if (moodExpiryMs > 0 && now >= moodExpiryMs) {
    currentMood = MOOD_IDLE;
    moodExpiryMs = 0;
    petRenderer.setShake(0);
  }

  // 5. CARATULA 2: SCREEN_WATCH (Smartwatch Chronograph & Complications)
  if (currentScreen == SCREEN_WATCH) {
    uint16_t touchX, touchY;
    if (gfx.getTouch(&touchX, &touchY) && now > lastScreenInputMs) {
      lastScreenInputMs = now + 380;
      // Top-Left "[< ROSTRO 2.5D]" or Bottom-Left "[ROSTRO 2.5D]" -> Switch to Watchface 1
      if ((touchY < 38 && touchX < 115) || (touchY >= 200 && touchX < 106)) {
        currentScreen = SCREEN_PET;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      // Top-Right "[WIFI ONLINE / MODO LOCAL]" -> Open Wi-Fi & Silicon Enclave
      if (touchY < 38 && touchX >= 210) {
        currentScreen = SCREEN_ENROLLMENT;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      // Bottom-Center "[GAFETE CEO]" -> Open Alonso Ruiz CEO Badge
      if (touchY >= 200 && touchX >= 106 && touchX < 214) {
        currentScreen = SCREEN_BADGE;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      // Bottom-Right "[TICKETS >]" or Complication Cards -> Open Incident Stream
      if ((touchY >= 200 && touchX >= 214) || (touchY >= 110 && touchY < 200)) {
        currentScreen = SCREEN_TICKET_LIST;
        petRenderer.clearScreen();
        soundFx.playTap();
        return;
      }
      soundFx.playTap();
    }

    bool wfOnline = (WiFi.status() == WL_CONNECTED);
    int rssi = wfOnline ? WiFi.RSSI() : 0;
#if USE_CYBER_FACE_RENDERER
    petRenderer.drawSmartwatchScreen(currentMood, latestTicketTitle, latestTicketId, activeTicketCount, resolvedTodayCount, wfOnline, rssi);
#endif
    updateUnderglow();
    delay(35);
    return;
  }

  // 6. CARATULA 1: SCREEN_PET (Full-Screen 2.5D Expressive Cyber-Face)
  uint16_t touchX, touchY;
  bool touched = gfx.getTouch(&touchX, &touchY);
#if USE_CYBER_FACE_RENDERER
  petRenderer.setTouch(touched, touchX, touchY);
#endif

  if (touched) {
    if (touchStartTime == 0) {
      touchStartTime = now;
      if (now > lastScreenInputMs) {
        // Top-Left "[RELOJ OS >]" -> Switch to Watchface 2 (Smartwatch Chronograph)!
        if (touchY < 38 && touchX < 115) {
          lastScreenInputMs = now + 380;
          currentScreen = SCREEN_WATCH;
          petRenderer.clearScreen();
          soundFx.playTap();
          return;
        }
        // Top-Right "[ACT / OK]" or Bottom-Right "[TICKETS >]" -> Open Ticket List
        else if ((touchY < 38 && touchX >= 210) || (touchY >= 200 && touchX >= 214)) {
          lastScreenInputMs = now + 380;
          currentScreen = SCREEN_TICKET_LIST;
          petRenderer.clearScreen();
          soundFx.playTap();
          return;
        }
        // Bottom-Left "[ALONSO CEO]" -> Open CEO Badge
        else if (touchY >= 200 && touchX < 106) {
          lastScreenInputMs = now + 380;
          currentScreen = SCREEN_BADGE;
          petRenderer.clearScreen();
          soundFx.playTap();
          return;
        }
        // Bottom-Center "[MODO: ...]" -> Cycle Facial Expression / Mood!
        else if (touchY >= 200 && touchX >= 106 && touchX < 214) {
          lastScreenInputMs = now + 380;
          cycleMoodInteractive();
          return;
        }
        // Center Visor -> Poke / Interact with the Eyes!
        else {
          petRenderer.wakeUp();
          currentMood = MOOD_POKED;
          moodExpiryMs = now + 1500;
          soundFx.playGrowl();
        }
      }
    } else if (now - touchStartTime > 2000) {
      // 2-second hold in center enters Wi-Fi & Silicon Enclave
      currentScreen = SCREEN_ENROLLMENT;
      petRenderer.clearScreen();
      touchStartTime = now + 1500;
      soundFx.playTap();
      return;
    }
  } else {
    touchStartTime = 0;
  }

  petRenderer.renderFrame(currentMood, latestTicketTitle, latestTicketId, latestTicketStatus, activeTicketCount, resolvedTodayCount, siliconMac);
  updateUnderglow();

  delay(35); // ~28-30 FPS smooth animation loop
}
