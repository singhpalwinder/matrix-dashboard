#include "now_playing.h"
#include "audio_levels.h"
#include "clock_view.h"
#include "config.h"
#include "image_view.h"

#include <math.h>

namespace now_playing {

namespace {

struct Rgb { float r, g, b; };

constexpr int   BARS        = 7;
constexpr int   BAR_W       = 3;
constexpr int   BAR_X0      = 34;   // 7 bars of 3px with 1px gaps span x=34..60
constexpr int   BAR_MAX_H   = 17;   // rows 15..31
constexpr float SPIN_RAD_S  = 0.8f; // about 8 seconds per turn
constexpr float SPIN_ACCEL  = 0.8f; // rad/s^2, so it spins up and down like a turntable
constexpr float BAR_FALL    = 40.f; // rows per second
constexpr float PEAK_FALL   = 12.f;
constexpr uint32_t PEAK_HOLD_MS = 300;

float    angle = 0, speed = 0;
uint32_t lastMs = 0;
float    shown[BARS] = {0}, peak[BARS] = {0};
uint32_t peakAt[BARS] = {0};

uint32_t paletteGen = UINT32_MAX;
Rgb      accent{255, 140, 40}, light{255, 220, 180};

inline Rgb unpack(uint16_t c) {
  return {(float)((c >> 11) & 0x1F) * (255.f / 31.f),
          (float)((c >> 5) & 0x3F) * (255.f / 63.f),
          (float)(c & 0x1F) * (255.f / 31.f)};
}
inline uint16_t pack(Rgb c) {
  auto ch = [](float v) { return (uint16_t)(v < 0 ? 0 : v > 255 ? 255 : v); };
  return dimRGB565((uint16_t)(((ch(c.r) & 0xF8) << 8) | ((ch(c.g) & 0xFC) << 3) | (ch(c.b) >> 3)));
}
inline Rgb mix(Rgb a, Rgb b, float k) {
  return {a.r + (b.r - a.r) * k, a.g + (b.g - a.g) * k, a.b + (b.b - a.b) * k};
}
inline uint16_t artPixel(const uint8_t* img, int x, int y) {
  int i = (y * 32 + x) * 2;
  return (uint16_t)((img[i] << 8) | img[i + 1]);
}

// Most vivid hue in the cover, brightened, for the time and bars.
void updatePalette(const uint8_t* img) {
  float wsum[12] = {0}, rs[12] = {0}, gs[12] = {0}, bs[12] = {0};
  for (int i = 0; i < 32 * 32; ++i) {
    Rgb c = unpack((uint16_t)((img[i * 2] << 8) | img[i * 2 + 1]));
    float mx = fmaxf(c.r, fmaxf(c.g, c.b)), mn = fminf(c.r, fminf(c.g, c.b));
    if (mx < 1) continue;
    float s = (mx - mn) / mx, v = mx / 255.f, w = s * v * v;
    if (w < 0.04f) continue;
    float d = mx - mn, h;
    if (mx == c.r)      h = fmodf((c.g - c.b) / d + 6.f, 6.f);
    else if (mx == c.g) h = (c.b - c.r) / d + 2.f;
    else                h = (c.r - c.g) / d + 4.f;
    int b = ((int)(h * 2.f)) % 12;
    wsum[b] += w; rs[b] += c.r * w; gs[b] += c.g * w; bs[b] += c.b * w;
  }
  int best = -1;
  for (int b = 0; b < 12; ++b) if (wsum[b] > 0 && (best < 0 || wsum[b] > wsum[best])) best = b;
  if (best < 0) {  // grey cover: neutral bars
    accent = {170, 180, 200};
  } else {
    accent = {rs[best] / wsum[best], gs[best] / wsum[best], bs[best] / wsum[best]};
    float mx = fmaxf(accent.r, fmaxf(accent.g, accent.b));
    float k = mx > 1 ? 235.f / mx : 1.f;
    accent = {accent.r * k, accent.g * k, accent.b * k};
  }
  light = mix(accent, {255, 255, 255}, 0.6f);
}

void drawRecord(Adafruit_GFX& g, const uint8_t* img) {
  const float c = cosf(angle), s = sinf(angle);
  for (int y = 0; y < 32; ++y) {
    for (int x = 0; x < 32; ++x) {
      float dx = x - 15.5f, dy = y - 15.5f, r = sqrtf(dx * dx + dy * dy);
      if (r > 15.9f || r < 1.8f) { g.drawPixel(x, y, 0); continue; }
      // Sample the cover rotated about its center, blending the 4 nearest pixels
      // so the spin glides instead of snapping between pixels.
      float sx = c * dx - s * dy + 15.5f, sy = s * dx + c * dy + 15.5f;
      sx = fminf(fmaxf(sx, 0.f), 31.f); sy = fminf(fmaxf(sy, 0.f), 31.f);
      int x0 = (int)sx, y0 = (int)sy, x1 = x0 < 31 ? x0 + 1 : 31, y1 = y0 < 31 ? y0 + 1 : 31;
      float fx = sx - x0, fy = sy - y0;
      Rgb top = mix(unpack(artPixel(img, x0, y0)), unpack(artPixel(img, x1, y0)), fx);
      Rgb bot = mix(unpack(artPixel(img, x0, y1)), unpack(artPixel(img, x1, y1)), fx);
      Rgb px = mix(top, bot, fy);
      if (r > 5.f && ((int)(r + 0.5f)) % 4 == 0) px = {px.r * 0.72f, px.g * 0.72f, px.b * 0.72f};  // grooves
      g.drawPixel(x, y, pack(px));
    }
  }
}

void drawBars(Adafruit_GFX& g, uint32_t nowMs, float dt) {
  uint8_t lv[audio_levels::MAX_BANDS];
  int n = audio_levels::latest(lv, audio_levels::MAX_BANDS, nowMs);
  uint16_t base = pack({accent.r * 0.25f, accent.g * 0.25f, accent.b * 0.25f});
  uint16_t cap  = pack(light);

  for (int i = 0; i < BARS; ++i) {
    float target = i < n ? lv[i] * (BAR_MAX_H / 255.f) : 0.f;
    shown[i] = target >= shown[i] ? target : fmaxf(target, shown[i] - BAR_FALL * dt);
    if (shown[i] >= peak[i]) { peak[i] = shown[i]; peakAt[i] = nowMs; }
    else if (nowMs - peakAt[i] > PEAK_HOLD_MS) peak[i] = fmaxf(shown[i], peak[i] - PEAK_FALL * dt);

    int x = BAR_X0 + i * (BAR_W + 1);
    int h = (int)(shown[i] + 0.5f);
    g.fillRect(x, 31, BAR_W, 1, base);  // resting line, so an idle panel still reads as bars
    for (int k = 0; k < h; ++k) {
      g.fillRect(x, 31 - k, BAR_W, 1, pack(mix(accent, light, k / (float)(BAR_MAX_H - 1))));
    }
    int p = (int)(peak[i] + 0.5f);
    if (p > h && p >= 1) g.fillRect(x, 31 - p, BAR_W, 1, cap);
  }
}

}  // namespace

void render(Adafruit_GFX& g, int hour12, int minute, uint32_t nowMs) {
  float dt = lastMs ? (nowMs - lastMs) / 1000.f : 0.f;
  if (dt > 0.2f) dt = 0.2f;
  lastMs = nowMs;

  const uint8_t* img = image_view::staticImage();
  if (img && image_view::imageGeneration() != paletteGen) {
    paletteGen = image_view::imageGeneration();
    updatePalette(img);
  }

  bool spinning = audio_levels::playing(nowMs) || audio_levels::playingElsewhere(nowMs);
  float targetSpeed = spinning ? SPIN_RAD_S : 0.f;
  if (speed < targetSpeed) speed = fminf(targetSpeed, speed + SPIN_ACCEL * dt);
  else                     speed = fmaxf(targetSpeed, speed - SPIN_ACCEL * dt);
  angle = fmodf(angle + speed * dt, 6.2831853f);

  if (img) drawRecord(g, img);
  else     g.fillRect(0, 0, 32, 32, 0);

  g.fillRect(32, 0, 32, 32, 0);
  clock_view::drawTimeSmall(g, 33, 63, 1, hour12, minute, pack(light));
  if (audio_levels::elsewhereLayout(nowMs)) {
    clock_view::drawDateStacked(g, 33, 63, 14, pack(accent));  // rows 14-20 and 23-29
  } else {
    drawBars(g, nowMs, dt);
  }
}

}  // namespace now_playing
