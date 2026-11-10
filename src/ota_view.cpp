#include "ota_view.h"
#include "config.h"
#include "digits6x11.h"
#include "ota.h"
#include "ota_anim.h"

#include <Fonts/TomThumb.h>
#include <string.h>

namespace ota_view {

static constexpr uint32_t FRAME_MS       = 70;
static constexpr uint32_t FAILED_SHOW_MS = 4000;

static constexpr uint16_t RED   = 0xF904;
static constexpr uint16_t WHITE = 0xFFFF;
static constexpr uint16_t GREEN = 0x4F6A;
static constexpr uint16_t TRACK = 0x2945;

bool active() { return ota::phase() != ota::Phase::IDLE; }

// Gears tile from flash; `tint` != 0 paints every lit pixel that color instead.
static void drawGears(Adafruit_GFX& g, int frame, uint16_t tint) {
  const uint8_t* f = OTA_ANIM[frame % OTA_ANIM_FRAMES];
  for (int i = 0; i < OTA_ANIM_SIZE * OTA_ANIM_SIZE; ++i) {
    uint8_t b = pgm_read_byte(&f[i >> 2]);
    uint8_t idx = (b >> (6 - 2 * (i & 3))) & 0x3;
    uint16_t col = idx ? (tint ? tint : OTA_ANIM_PALETTE[idx]) : 0;
    g.drawPixel(i % OTA_ANIM_SIZE, i / OTA_ANIM_SIZE, dimRGB565(col));
  }
}

static void label(Adafruit_GFX& g, const char* s, int baseline, uint16_t color) {
  g.setFont(&TomThumb);
  g.setTextSize(1);
  g.setTextColor(dimRGB565(color));
  int w = (int)strlen(s) * 4 - 1;
  g.setCursor(32 + (32 - w) / 2, baseline);
  g.print(s);
  g.setFont(nullptr);
}

static void percent(Adafruit_GFX& g, int pct, uint16_t color) {
  char buf[5];
  snprintf(buf, sizeof(buf), "%d", pct);
  int w = (int)strlen(buf) * 7 - 1 + 1 + 3;  // digits, gap, 3px-wide '%'
  int x = 32 + (32 - w) / 2;
  g.setFont(&Digits6x11);
  g.setTextColor(dimRGB565(color));
  g.setCursor(x, 9 + 11);
  g.print(buf);
  g.setFont(&TomThumb);
  g.setCursor(x + (int)strlen(buf) * 7, 9 + 11);
  g.print('%');
  g.setFont(nullptr);
}

static void bar(Adafruit_GFX& g, float frac, uint16_t color) {
  constexpr int X = 34, W = 28, Y = 25, H = 3;
  int fill = (int)(frac * W + 0.5f);
  if (fill > W) fill = W;
  g.fillRect(X, Y, W, H, dimRGB565(TRACK));
  if (fill > 0) g.fillRect(X, Y, fill, H, dimRGB565(color));
}

uint8_t render(Adafruit_GFX& g, uint32_t nowMs) {
  ota::Phase p = ota::phase();
  int frame = (int)(nowMs / FRAME_MS);
  uint32_t tot = ota::total();
  float frac = tot ? (float)ota::received() / (float)tot : 0.f;

  g.fillScreen(0);
  switch (p) {
    case ota::Phase::ERASING:
      // Stay black: the erase stalls the refresh, and a stalled black frame is invisible.
      break;
    case ota::Phase::WRITING:
      drawGears(g, frame, 0);
      label(g, "UPDATING", 7, WHITE);
      percent(g, (int)(frac * 100.f), WHITE);
      bar(g, frac, OTA_ANIM_PALETTE[1]);
      break;
    case ota::Phase::VERIFYING:
      drawGears(g, frame, 0);
      label(g, "CHECKING", 7, WHITE);
      percent(g, 100, WHITE);
      bar(g, 1.f, OTA_ANIM_PALETTE[1]);
      break;
    case ota::Phase::DONE:
      drawGears(g, 0, GREEN);
      label(g, "RESTART", 7, GREEN);
      percent(g, 100, GREEN);
      bar(g, 1.f, GREEN);
      break;
    case ota::Phase::FAILED:
      drawGears(g, 0, RED);
      label(g, "FAILED", 7, RED);
      label(g, "OLD FW", 19, WHITE);
      label(g, "KEPT", 27, WHITE);
      if (nowMs - ota::phaseStartedMs() > FAILED_SHOW_MS) ota::clearFailed();
      break;
    case ota::Phase::IDLE:
      break;
  }
  return (uint8_t)p;
}

}  // namespace ota_view
