#include "clock_view.h"
#include "config.h"
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/Picopixel.h>
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

void renderDigital(Adafruit_GFX& g, int hour12, int minute, bool am, uint16_t textColor) {
  char timeBuf[8];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d", hour12, minute);

  g.fillScreen(COL_BG);
  g.setTextWrap(false);
  g.setTextColor(textColor);

  g.setFont(&FreeSans12pt7b);
  constexpr int RESERVE_RIGHT = 12;
  int16_t x1, y1;
  uint16_t w, h;
  g.getTextBounds(timeBuf, 0, 0, &x1, &y1, &w, &h);
  // Subtract x1 (not a magic 2) so glyphs with non-zero bearing — e.g. the
  // "1" in "10:00" — don't get clipped off the left.
  int tx = ((M_W - RESERVE_RIGHT) - (int)w) / 2 - x1;
  int ty = (M_H - (int)h) / 2 - y1;
  g.setCursor(tx, ty);
  g.print(timeBuf);

  g.setFont(&Picopixel);
  const char* meridian = am ? "AM" : "PM";
  g.getTextBounds(meridian, 0, 0, &x1, &y1, &w, &h);
  int mx = M_W - 1 - (int)w - x1;
  int my = (M_H - (int)h) / 2 - y1;
  g.setCursor(mx, my);
  g.print(meridian);
}

}  // namespace clock_view
