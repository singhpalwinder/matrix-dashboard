#include "clock_view.h"
#include "config.h"
#include <Fonts/FreeSans9pt7b.h>
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

static void drawFace(Adafruit_GFX& g) {
  g.drawCircle(CLOCK_CX, CLOCK_CY, CLOCK_R, COL_FACE);
  g.fillCircle(CLOCK_CX, CLOCK_CY, 1, COL_FACE);

  for (int i = 0; i < 12; i++) {
    int angle = (i * 30) - 90;
    if (angle < 0) angle += 360;
    int c = cosLUT[angle];
    int s = sinLUT[angle];
    int x1 = CLOCK_CX + scaleQ10(c, CLOCK_R - 1);
    int y1 = CLOCK_CY + scaleQ10(s, CLOCK_R - 1);
    int x2 = CLOCK_CX + scaleQ10(c, CLOCK_R - 3);
    int y2 = CLOCK_CY + scaleQ10(s, CLOCK_R - 3);
    g.drawLine(x1, y1, x2, y2, COL_TICK);
  }
}

void renderAnalog(Adafruit_GFX& g, int hour12, int minute) {
  // Blank the right half first so transitions / overlays are clean.
  g.fillRect(M_W - 32, 0, 32, M_H, COL_BG);
  drawFace(g);

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

  g.drawLine(CLOCK_CX, CLOCK_CY, hx, hy, COL_HAND);
  g.drawLine(CLOCK_CX, CLOCK_CY, mx, my, COL_HAND);
}

void renderDigital(Adafruit_GFX& g, int hour12, int minute, bool am, uint16_t textColor) {
  char timeBuf[12];
  snprintf(timeBuf, sizeof(timeBuf), "%d:%02d%s", hour12, minute, am ? "AM" : "PM");

  g.fillScreen(COL_BG);
  g.setFont(&FreeSans9pt7b);
  g.setTextWrap(false);
  g.setTextColor(textColor);

  int16_t x1, y1;
  uint16_t w, h;
  g.getTextBounds(timeBuf, 0, 0, &x1, &y1, &w, &h);
  int tx = (M_W - (int)w) / 2 - 2;
  int ty = (M_H + (int)h) / 2 - 2;
  g.setCursor(tx, ty);
  g.print(timeBuf);
}

}  // namespace clock_view
