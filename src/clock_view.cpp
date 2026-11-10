#include "clock_view.h"
#include "config.h"
#include "digits6x11.h"
#include <string.h>
#include <math.h>

namespace clock_view {

static int16_t sinLUT[360];
static int16_t cosLUT[360];

// RGB565: white face by default; hand/tick color is the orange accent.
static uint16_t COL_FACE = 0xFFFF;
static constexpr uint16_t COL_TICK = 0xFB80;  // ~color565(250,119,0)
static constexpr uint16_t COL_HAND = 0xFB80;
static constexpr uint16_t COL_BG   = 0x0000;

void buildTrigLUT() {
  for (int a = 0; a < 360; ++a) {
    double rad = (double)a * (M_PI / 180.0);
    sinLUT[a] = (int16_t)(sin(rad) * 1024.0);
    cosLUT[a] = (int16_t)(cos(rad) * 1024.0);
  }
}

static inline int scaleQ10(int s, int len) {
  // (s * len) with s in Q10 -> pixels (rounded toward zero)
  return ((int32_t)s * (int32_t)len) >> 10;
}

void setFaceColor(uint16_t c) { COL_FACE = c; }
uint16_t faceColor() { return COL_FACE; }

static void drawFace(Adafruit_GFX& g, uint16_t faceCol, uint16_t tickCol) {
  g.drawCircle(CLOCK_CX, CLOCK_CY, CLOCK_R, faceCol);
  g.fillCircle(CLOCK_CX, CLOCK_CY, 1, faceCol);

  for (int i = 0; i < 12; i++) {
    int angle = (i * 30) - 90;
    if (angle < 0) angle += 360;
    int c = cosLUT[angle];
    int s = sinLUT[angle];
    int x1 = CLOCK_CX + scaleQ10(c, CLOCK_R - 1);
    int y1 = CLOCK_CY + scaleQ10(s, CLOCK_R - 1);
    int x2 = CLOCK_CX + scaleQ10(c, CLOCK_R - 3);
    int y2 = CLOCK_CY + scaleQ10(s, CLOCK_R - 3);
    g.drawLine(x1, y1, x2, y2, tickCol);
  }
}

void renderAnalog(Adafruit_GFX& g, int hour12, int minute) {
  // Pre-dim once so the per-pixel drawCircle/drawLine work uses cheap colors.
  uint16_t faceCol = dimRGB565(COL_FACE);
  uint16_t tickCol = dimRGB565(COL_TICK);
  uint16_t handCol = dimRGB565(COL_HAND);

  // Blank the right half first so transitions / overlays are clean.
  g.fillRect(M_W - 32, 0, 32, M_H, COL_BG);
  drawFace(g, faceCol, tickCol);

  int hourAngle   = ((hour12 % 12) * 30) + (minute / 2) - 90;
  if (hourAngle < 0) hourAngle += 360;
  int minuteAngle = (minute * 6) - 90;
  if (minuteAngle < 0) minuteAngle += 360;

  int ch = cosLUT[hourAngle];
  int sh = sinLUT[hourAngle];
  int cm = cosLUT[minuteAngle];
  int sm = sinLUT[minuteAngle];

  int hx = CLOCK_CX + scaleQ10(ch, CLOCK_R - 7);
  int hy = CLOCK_CY + scaleQ10(sh, CLOCK_R - 7);
  int mx = CLOCK_CX + scaleQ10(cm, CLOCK_R - 5);
  int my = CLOCK_CY + scaleQ10(sm, CLOCK_R - 5);

  g.drawLine(CLOCK_CX, CLOCK_CY, hx, hy, handCol);
  g.drawLine(CLOCK_CX, CLOCK_CY, mx, my, handCol);
}

static char g_date[16] = "";
static bool g_compact = false;

void setCompact(bool compact) { g_compact = compact; }

void setDate(const char* text) {
  strncpy(g_date, text ? text : "", sizeof(g_date) - 1);
  g_date[sizeof(g_date) - 1] = 0;
}

static void timeString(char* buf, size_t n, int hour12, int minute) {
  if (hour12 < 0) snprintf(buf, n, "--:--");
  else            snprintf(buf, n, "%d:%02d", hour12, minute);
}

// Width of a Digits6x11 string at 1x: sum of advances minus the trailing gap.
static int digitsWidth(const char* s) {
  int w = 0;
  for (; *s; ++s) {
    uint8_t c = (uint8_t)*s;
    if (c < Digits6x11.first || c > Digits6x11.last) continue;
    w += pgm_read_byte(&Digits6x11Glyphs[c - Digits6x11.first].xAdvance);
  }
  return w > 0 ? w - 1 : 0;
}

static void printDigits(Adafruit_GFX& g, const char* s, int x, int top, int scale, uint16_t color) {
  g.setFont(&Digits6x11);
  g.setTextSize(scale);
  g.setTextWrap(false);
  g.setTextColor(color);
  g.setCursor(x, top + 11 * scale);  // cursor sits on the baseline (bottom row + 1)
  g.print(s);
  g.setTextSize(1);
  g.setFont(nullptr);
}

// Layout. Compact (Pi tiles showing): time 1x in the top-left quarter, date across
// the middle. Big: time 2x (12x22) at the top, date along the bottom.
static constexpr int COMPACT_DATE_Y = 14;
static constexpr int BIG_DATE_Y     = 24;

static bool hasDate() { return g_date[0] != 0; }

void drawTime(Adafruit_GFX& g, int hour12, int minute, uint16_t color) {
  char buf[8];
  timeString(buf, sizeof(buf), hour12, minute);
  if (g_compact) {
    printDigits(g, buf, (32 - digitsWidth(buf)) / 2, 1, 1, color);
    return;
  }
  constexpr int SCALE = 2;
  bool withDate = hasDate() && hour12 >= 0;
  int top = withDate ? 1 : (M_H - 11 * SCALE) / 2;
  printDigits(g, buf, (M_W - digitsWidth(buf) * SCALE) / 2, top, SCALE, color);
}

void drawDate(Adafruit_GFX& g, uint16_t color) {
  if (!hasDate()) return;
  // Built-in 5x7 font: 6px per character, 7 rows tall.
  int dw = (int)strlen(g_date) * 6 - 1;
  g.setFont(nullptr);
  g.setTextSize(1);
  g.setTextWrap(false);
  g.setTextColor(color);
  g.setCursor((M_W - dw) / 2, g_compact ? COMPACT_DATE_Y : BIG_DATE_Y);
  g.print(g_date);
}

bool inTimeArea(int x, int y) {
  if (g_compact) return x < 32 && y < COMPACT_DATE_Y - 1;
  return y < BIG_DATE_Y - 1;
}

void drawFace(Adafruit_GFX& g, int hour12, int minute, uint16_t color) {
  drawTime(g, hour12, minute, color);
  if (hour12 >= 0) drawDate(g, color);
}

void drawTimeSmall(Adafruit_GFX& g, int x0, int x1, int y, int hour12, int minute, uint16_t color) {
  char buf[8];
  timeString(buf, sizeof(buf), hour12, minute);
  int w = digitsWidth(buf);
  printDigits(g, buf, x0 + (x1 - x0 + 1 - w) / 2, y, 1, color);
}

void renderDigital(Adafruit_GFX& g, int hour12, int minute, bool /*am*/, uint16_t textColor) {
  g.fillScreen(COL_BG);
  drawFace(g, hour12, minute, textColor);
}

}  // namespace clock_view
