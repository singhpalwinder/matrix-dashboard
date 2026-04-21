#include <Arduino.h>
#include <WiFi.h>
#include <ezTime.h>
#include <Adafruit_SPIFlash.h>
#include <SdFat.h>
#include <Fonts/FreeSans9pt7b.h>

#include "secrets.h"
#include "config.h"
#include "app_state.h"
#include "display.h"
#include "clock_view.h"
#include "image_view.h"
#include "transitions.h"
#include "net_server.h"
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
        clock_view::renderDigital(g, h12, mins, am, g_textColor);
      } else {
        // Time not synced yet. Show a placeholder so the display isn't blank.
        g.fillScreen(0);
        g.setFont(&FreeSans9pt7b);
        g.setTextWrap(false);
        g.setTextColor(g_textColor);
        g.setCursor(10, 22);
        g.print("--:--");
      }
      break;

    case Mode::IMAGE_STATIC:
    case Mode::IMAGE_GIF:
      g.fillScreen(0);
      image_view::renderImage(g);
      clock_view::renderAnalog(g, h12, mins);
      break;
  }
}

static bool connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED &&
         (millis() - start) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
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

  // Show something immediately so boot feels responsive even if WiFi is slow.
  clock_view::renderDigital(matrix, 12, 0, true, g_textColor);
  matrix.show();

  bool wifiOk = connectWiFi();
  if (wifiOk) {
    waitForSync(NTP_SYNC_TIMEOUT_S);
    chicago.setLocation(TIMEZONE);
    netServerBegin();
  }

  flash.begin();
  fatfs.begin(&flash);

  g_needsRedraw = true;
}

void loop() {
  events();  // ezTime housekeeping (NTP retries, etc.)

  if (g_turnOff) {
    delay(10);
    return;
  }
  if (g_uploadInProgress || g_processingRequest) {
    delay(1);
    return;
  }

  uint32_t now = millis();

  // GIF frame advance even when idle so animation keeps playing.
  if (image_view::tickFrameAdvance(now)) {
    g_needsRedraw = true;
  }

  // Detect view change and arm a dissolve. The "from" side is implicitly
  // whatever is currently in the live framebuffer (the last rendered view),
  // so we only need to paint the destination snapshot.
  Mode nowMode = currentMode();
  if (nowMode != lastMode && !transitions::active()) {
    renderForMode(nowMode, transitions::toSurface());
    transitions::start();
    lastMode = nowMode;
  }

  if (transitions::active()) {
    transitions::step();
    matrix.show();
    lastRenderMs = now;
    return;
  }

  bool secondTick = (now - lastRenderMs) >= 1000;
  if (!g_needsRedraw && !secondTick) {
    delay(1);
    return;
  }

  renderForMode(nowMode, matrix);
  matrix.show();

  lastRenderMs  = now;
  g_needsRedraw = false;
}
