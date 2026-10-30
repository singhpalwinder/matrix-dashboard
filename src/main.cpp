#include <Arduino.h>
#include <WiFi.h>
#include <ezTime.h>
#include <Adafruit_SPIFlash.h>
#include <SdFat.h>
#include <Fonts/FreeSans12pt7b.h>

#include "secrets.h"
#include "config.h"
#include "app_state.h"
#include "display.h"
#include "clock_view.h"
#include "image_view.h"
#include "transitions.h"
#include "clock_anim.h"
#include "net_server.h"
#include "mqtt_client.h"
#include "prefs_store.h"

// Shared state definitions. See app_state.h for the contract.
volatile bool     g_needsRedraw       = false;
volatile bool     g_turnOff           = false;
volatile bool     g_uploadInProgress  = false;
volatile bool     g_processingRequest = false;
volatile uint16_t g_textColor         = 0;

// Retained for the legacy FAT icon path (unused at runtime today but the
// storage is initialized so future features can read files).
static Adafruit_FlashTransport_ESP32 flashTransport;
static Adafruit_SPIFlash flash(&flashTransport);
static FatVolume fatfs;

static Timezone chicago;

enum class Mode : uint8_t { DIGITAL, IMAGE_STATIC, IMAGE_GIF };

static Mode     lastMode        = Mode::DIGITAL;
static uint32_t lastRenderMs    = 0;
static int      lastDigitalMin  = -1;
static int      lastImageMin    = -1;

static Mode currentMode() {
  if (image_view::hasGif())   return Mode::IMAGE_GIF;
  if (image_view::hasImage()) return Mode::IMAGE_STATIC;
  return Mode::DIGITAL;
}

static void renderForMode(Mode m, Adafruit_GFX& g) {
  int h12 = 12, mins = 0;
  bool am = true;
  bool haveTime = (timeStatus() != timeNotSet);
  if (haveTime) {
    h12  = chicago.hourFormat12();
    mins = chicago.minute();
    am   = chicago.hour() < 12;
  }

  switch (m) {
    case Mode::DIGITAL:
      if (haveTime) {
        // Render in the active animation style's static palette so the clock
        // matches the minute-change animation's look immediately, not just
        // after the first minute tick.
        clock_anim::renderColored(g, h12, mins, am);
      } else {
        // Time not synced yet. Show a placeholder so the display isn't blank.
        g.fillScreen(0);
        g.setFont(&FreeSans12pt7b);
        g.setTextWrap(false);
        g.setTextColor(dimRGB565(g_textColor));
        int16_t bx, by; uint16_t bw, bh;
        g.getTextBounds("--:--", 0, 0, &bx, &by, &bw, &bh);
        g.setCursor((M_W - (int)bw) / 2 - bx, (M_H - (int)bh) / 2 - by);
        g.print("--:--");
      }
      break;

    case Mode::IMAGE_STATIC:
    case Mode::IMAGE_GIF:
      // No fillScreen here: renderImage paints the full left 32x32 and
      // renderAnalog clears+paints the full right 32x32, so a global clear
      // is redundant and just widens the mid-frame paint window.
      image_view::renderImage(g);
      clock_view::renderAnalog(g, h12, mins);
      break;
  }
}

static bool tryJoin(const char* ssid, const char* pass) {
  if (!ssid || ssid[0] == '\0') return false;
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, pass);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool connectWiFi() {
  // Portal-saved creds take priority over the compile-time secrets so the
  // user can reconfigure a deployed board without reflashing.
  String savedSsid, savedPass;
  if (loadString(KEY_WIFI_SSID, savedSsid) && savedSsid.length() > 0) {
    loadString(KEY_WIFI_PASS, savedPass);
    if (tryJoin(savedSsid.c_str(), savedPass.c_str())) return true;
  }
  return tryJoin(WIFI_SSID, WIFI_PASS);
}

static void startAccessPoint() {
  WiFi.mode(WIFI_AP);
  if (AP_PASS[0] == '\0') WiFi.softAP(AP_SSID);
  else                    WiFi.softAP(AP_SSID, AP_PASS);
}

void setup() {
  Serial.begin(115200);
  delay(50);

  clock_view::buildTrigLUT();

  prefsBegin();
  uint16_t textCol = 0;
  if (loadColor(KEY_TEXT_COLOR, textCol)) {
    g_textColor = textCol;
  } else {
    g_textColor = 0x94B2;  // ~color565(150,150,150); gray default
    saveColor(KEY_TEXT_COLOR, g_textColor);
  }
  uint16_t faceCol = 0;
  if (loadColor(KEY_FACE_COLOR, faceCol)) {
    clock_view::setFaceColor(faceCol);
  }

  displayBegin();
  matrix.setTextColor(g_textColor);

  clock_anim::begin();

  // Show something immediately so boot feels responsive even if WiFi is slow.
  // Use the active animation style so the very first frame matches the look
  // of subsequent minute-change animations.
  clock_anim::renderColored(matrix, 12, 0, true);
  matrix.show();

  // Configure an offline-usable Posix TZ up front so manual time-sets in AP
  // mode already land in the right local zone; setLocation() overrides this
  // with IANA data once NTP/HTTP work.
  chicago.setPosix(TIMEZONE_POSIX);

  bool wifiOk = connectWiFi();
  if (wifiOk) {
    waitForSync(NTP_SYNC_TIMEOUT_S);
    chicago.setLocation(TIMEZONE);
    netSetApMode(false);
    mqttBegin();
  } else {
    startAccessPoint();
    netSetApMode(true);
  }
  netServerBegin();

  flash.begin();
  fatfs.begin(&flash);

  g_needsRedraw = true;
}

void loop() {
  events();  // ezTime housekeeping (NTP retries, etc.)

  clock_anim::pollButtons();

  if (g_turnOff) {
    delay(10);
    return;
  }
  if (g_uploadInProgress || g_processingRequest) {
    delay(1);
    return;
  }

  uint32_t now = millis();

  // Hoist time queries: each of these touches ezTime/now() internally,
  // and we'd otherwise call them 3x in this loop iteration.
  bool haveTime = (timeStatus() != timeNotSet);
  int  curMin   = haveTime ? chicago.minute() : -1;

  // GIF frame advance even when idle so animation keeps playing.
  if (image_view::tickFrameAdvance(now)) {
    g_needsRedraw = true;
  }

  // Detect view change and arm a dissolve. The "from" side is implicitly
  // whatever is currently in the live framebuffer (the last rendered view),
  // so we only need to paint the destination snapshot.
  Mode nowMode = currentMode();
  if (nowMode != lastMode && !transitions::active() &&
      !clock_anim::active()) {
    renderForMode(nowMode, transitions::toSurface());
    transitions::start();
    lastMode = nowMode;
    // After a mode change the next digital render should be a clean draw,
    // not a clock animation against whatever pixels happen to be in the
    // buffer.
    lastDigitalMin = -1;
    lastImageMin   = -1;
  }

  if (transitions::active()) {
    transitions::step();
    matrix.show();
    lastRenderMs = now;
    return;
  }

  // Digital-mode minute change → kick off the active animation style.
  if (nowMode == Mode::DIGITAL && !clock_anim::active() && haveTime) {
    if (lastDigitalMin != -1 && curMin != lastDigitalMin) {
      int  h12 = chicago.hourFormat12();
      bool am  = chicago.hour() < 12;
      clock_anim::startForDigital(h12, curMin, am, g_textColor);
      lastDigitalMin = curMin;
    }
  }

  if (clock_anim::active()) {
    clock_anim::step();
    matrix.show();
    lastRenderMs = now;
    return;
  }

  // In digital mode we don't repaint every second: the minute-change tetris
  // animation writes retro-colored pixels into the live buffer, and we want
  // those colors to persist until the next minute. We still redraw when
  // something changes (g_needsRedraw) or on the very first digital render.
  bool firstDigital = (nowMode == Mode::DIGITAL && lastDigitalMin == -1 &&
                       haveTime);

  // Image modes only need a repaint when the minute hand actually moves.
  // GIF frame advances and uploads still set g_needsRedraw, so animation
  // remains responsive — we just stop the wasteful 1Hz full repaint that
  // was driving the flicker.
  bool imageMinuteTick = false;
  if (nowMode != Mode::DIGITAL && haveTime) {
    if (lastImageMin == -1)            imageMinuteTick = true;  // first paint
    else if (curMin != lastImageMin)   imageMinuteTick = true;
  }

  if (!g_needsRedraw && !firstDigital && !imageMinuteTick) {
    delay(1);
    return;
  }

  renderForMode(nowMode, matrix);
  matrix.show();

  if (haveTime) {
    if (nowMode == Mode::DIGITAL) lastDigitalMin = curMin;
    else                          lastImageMin   = curMin;
  }

  lastRenderMs  = now;
  g_needsRedraw = false;
}
