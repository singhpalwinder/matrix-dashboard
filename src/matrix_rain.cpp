#include "matrix_rain.h"
#include "config.h"
#include "display.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/Picopixel.h>
#include <Arduino.h>
#include <string.h>

// "The Matrix" style minute-change animation. Mechanically tetris-like:
//
//   - Each lit OLD-frame pixel becomes a single-pixel raindrop falling
//     straight off the bottom of the panel.
//   - Each lit NEW-frame pixel becomes a raindrop that starts above the
//     panel, falls down, and locks at its target position (the "anchor")
//     in bright matrix green.
//   - Atmospheric background raindrops fall in random columns to keep the
//     surrounding screen alive while the new time settles in.
//
// All three kinds are rendered with a fading green head + trail. Animation
// ends when no drop is still in motion — same termination semantics as the
// tetris path.
namespace matrix_rain {

static GFXcanvas16 targetCanvas(M_W, M_H);

static constexpr int MASK_BYTES = (PIX_COUNT + 7) / 8;
static uint8_t outMask[MASK_BYTES];
static uint8_t inMask[MASK_BYTES];

static inline bool mget(const uint8_t* m, int idx) {
  return (m[idx >> 3] >> (idx & 7)) & 1;
}
static inline void mset(uint8_t* m, int idx) {
  m[idx >> 3] |= (uint8_t)(1u << (idx & 7));
}

// RGB565 color stops along the trail.
static constexpr uint16_t COLOR_HEAD     = 0xCFFD;  // near-white with green
static constexpr uint16_t COLOR_TRAIL_HI = 0x07E0;  // bright green
static constexpr uint16_t COLOR_TRAIL_MID= 0x0420;  // mid green
static constexpr uint16_t COLOR_TRAIL_LO = 0x0140;  // dim green
static constexpr uint16_t COLOR_LOCKED   = 0x07E0;  // settled "anchor" pixel
static constexpr uint16_t COLOR_FLASH    = 0xFFFF;  // full white — transition pop
static constexpr int      TRAIL_LEN      = 5;

// Transition windows (ms): OUT pixels brighten in their last `OUT_PRE_HEAD_MS`
// of pre-launch and pop white in the last `OUT_PRE_FLASH_MS`. IN drops flash
// white for `IN_LAND_FLASH_MS` after landing, then ease through white-green
// for another `IN_LAND_SETTLE_MS` before settling at the final green.
static constexpr uint32_t OUT_PRE_HEAD_MS   = 90;
static constexpr uint32_t OUT_PRE_FLASH_MS  = 35;
static constexpr uint32_t IN_LAND_FLASH_MS  = 70;
static constexpr uint32_t IN_LAND_SETTLE_MS = 160;

enum Kind : uint8_t { KIND_IN = 0, KIND_OUT = 1, KIND_BG = 2 };

struct Drop {
  int8_t   x;
  int16_t  y_start_q;          // Q4 starting head-y
  int16_t  y_target_q;         // Q4 final head-y (where IN locks; off-bottom otherwise)
  uint32_t start_ms;
  uint16_t fall_ms_per_cell;
  uint8_t  kind;
};

// Worst-case sizing: every lit pixel of the old + new frame plus the BG
// raindrops. 9pt sans digits clock in around 250–300 lit pixels each, so
// 800 leaves comfortable headroom — overflow here meant the IN drops were
// being silently skipped, which made the new time appear instantly at end
// of animation instead of falling in as raindrops.
static constexpr int  MAX_DROPS = 800;
static Drop           drops[MAX_DROPS];
static int            numDrops  = 0;

// Two-speed timing: OUT and BG drops rain quickly so the surrounding screen
// reads as atmospheric noise, while IN drops fall noticeably slower so the
// eye can follow each new-time pixel down to its anchor and watch it lock.
//
// BG rain is intentionally spread well past the latest IN landing so the
// screen keeps raining around the freshly-settled time for a beat before
// going quiet — that "hold" is what makes the transition land.
static constexpr uint32_t FAST_MS_BASE   = 30;     // OUT / BG
static constexpr uint32_t FAST_MS_JITTER = 30;     // 30..59 ms/cell
static constexpr uint32_t SLOW_MS_BASE   = 75;     // IN
static constexpr uint32_t SLOW_MS_JITTER = 25;     // 75..99 ms/cell
static constexpr uint32_t OUT_STAGGER_MS = 1200;
static constexpr uint32_t IN_DELAY_MS    = 300;
static constexpr uint32_t IN_STAGGER_MS  = 900;
static constexpr uint32_t BG_STAGGER_MS  = 2600;   // extends rain past IN landings
static constexpr int      BG_DROPS_HINT  = 110;

static uint32_t startMs  = 0;
static bool     isActive = false;

static uint32_t xs = 0xC0FFEEEEu;
static inline uint32_t xs32() {
  uint32_t x = xs;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  xs = x;
  return x;
}

static void renderTargetCanvas(int hour12, int minute, bool am) {
  targetCanvas.fillScreen(0x0000);
  targetCanvas.setTextWrap(false);
  targetCanvas.setTextColor(0xFFFF);

  // "h:mm" big — regular 12pt sans.
  targetCanvas.setFont(&FreeSans12pt7b);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", hour12, minute);

  constexpr int RESERVE_RIGHT = 12;
  int16_t bx, by;
  uint16_t bw, bh;
  targetCanvas.getTextBounds(buf, 0, 0, &bx, &by, &bw, &bh);
  // Subtract bx (not a magic 2) so glyphs whose bounds start at non-zero
  // offsets — e.g. the "1" in "10:00" — don't get clipped off the left.
  int tx = ((M_W - RESERVE_RIGHT) - (int)bw) / 2 - bx;
  int ty = (M_H - (int)bh) / 2 - by;
  targetCanvas.setCursor(tx, ty);
  targetCanvas.print(buf);

  // "AM"/"PM" tiny, right-aligned, vertically centered.
  targetCanvas.setFont(&Picopixel);
  const char* meridian = am ? "AM" : "PM";
  targetCanvas.getTextBounds(meridian, 0, 0, &bx, &by, &bw, &bh);
  int mx = M_W - 1 - (int)bw - bx;
  int my = (M_H - (int)bh) / 2 - by;
  targetCanvas.setCursor(mx, my);
  targetCanvas.print(meridian);
}

// Color lookup: how bright is `dist` cells behind the head?
static inline uint16_t trailColor(int dist) {
  if (dist == 0) return COLOR_HEAD;
  if (dist <= 1) return COLOR_TRAIL_HI;
  if (dist <= 3) return COLOR_TRAIL_MID;
  if (dist <= TRAIL_LEN) return COLOR_TRAIL_LO;
  return 0x0000;
}

void startForDigital(int hour12, int minute, bool am, uint16_t /*textColor*/) {
  constexpr uint16_t BG = 0x0000;

  renderTargetCanvas(hour12, minute, am);

  const uint16_t* live = matrix.getBuffer();
  const uint16_t* tgt  = targetCanvas.getBuffer();
  memset(outMask, 0, sizeof(outMask));
  memset(inMask,  0, sizeof(inMask));
  for (int i = 0; i < PIX_COUNT; ++i) {
    if (live[i] != BG) mset(outMask, i);
    if (tgt[i]  != BG) mset(inMask,  i);
  }

  numDrops = 0;
  xs ^= millis();

  // OUT: each lit old pixel → raindrop falling straight down off bottom.
  for (int y = 0; y < M_H && numDrops < MAX_DROPS; ++y) {
    for (int x = 0; x < M_W && numDrops < MAX_DROPS; ++x) {
      if (!mget(outMask, y * M_W + x)) continue;
      Drop& d = drops[numDrops++];
      d.x                = (int8_t)x;
      d.y_start_q        = (int16_t)(y << 4);
      d.y_target_q       = (int16_t)((M_H + TRAIL_LEN + 1) << 4);
      d.fall_ms_per_cell =
          (uint16_t)(FAST_MS_BASE + (xs32() % FAST_MS_JITTER));
      d.start_ms         = xs32() % OUT_STAGGER_MS;
      d.kind             = KIND_OUT;
    }
  }

  // BG: atmospheric rain in random columns, falling from above off bottom.
  for (int i = 0; i < BG_DROPS_HINT && numDrops < MAX_DROPS; ++i) {
    Drop& d = drops[numDrops++];
    d.x                = (int8_t)(xs32() % M_W);
    int above          = 1 + (int)(xs32() % 4);    // 1..4 cells above panel
    d.y_start_q        = (int16_t)(-(above << 4));
    d.y_target_q       = (int16_t)((M_H + TRAIL_LEN + 1) << 4);
    d.fall_ms_per_cell =
        (uint16_t)(FAST_MS_BASE + (xs32() % FAST_MS_JITTER));
    d.start_ms         = xs32() % BG_STAGGER_MS;
    d.kind             = KIND_BG;
  }

  // IN: each lit new pixel → raindrop falling from above to its anchor.
  // Slower fall speed than OUT/BG so each new-time pixel is clearly readable
  // as it descends and snaps into place. Generated last so they render on
  // top of any colliding bg/out trails.
  for (int y = 0; y < M_H && numDrops < MAX_DROPS; ++y) {
    for (int x = 0; x < M_W && numDrops < MAX_DROPS; ++x) {
      if (!mget(inMask, y * M_W + x)) continue;
      Drop& d = drops[numDrops++];
      d.x                = (int8_t)x;
      int above          = 2 + (int)(xs32() % 5);   // 2..6 cells above panel
      d.y_start_q        = (int16_t)(-(above << 4));
      d.y_target_q       = (int16_t)(y << 4);
      d.fall_ms_per_cell =
          (uint16_t)(SLOW_MS_BASE + (xs32() % SLOW_MS_JITTER));
      d.start_ms         = IN_DELAY_MS + (xs32() % IN_STAGGER_MS);
      d.kind             = KIND_IN;
    }
  }

  startMs  = millis();
  isActive = true;
}

bool active() { return isActive; }

// Draw a single raindrop's head + trail into `live`. The trail length is
// clipped to how far the drop has actually fallen since launch, so a drop
// at the moment of launch is just a head — the trail then grows behind it
// as it falls. For locked drops (OUT pre-launch, IN landed) only the anchor
// pixel renders, in `lockedColor` so callers can stage transition flashes.
static inline void drawDrop(uint16_t* live, int x, int head_y, int dist_traveled,
                            bool locked, uint16_t lockedColor) {
  if (locked) {
    if (head_y >= 0 && head_y < M_H && x >= 0 && x < M_W) {
      live[head_y * M_W + x] = lockedColor;
    }
    return;
  }
  if (x < 0 || x >= M_W) return;
  int max_d = (dist_traveled < TRAIL_LEN) ? dist_traveled : TRAIL_LEN;
  if (max_d < 0) max_d = 0;
  for (int d = 0; d <= max_d; ++d) {
    int cy = head_y - d;
    if (cy < 0 || cy >= M_H) continue;
    uint16_t col = trailColor(d);
    if (col == 0) continue;
    live[cy * M_W + x] = col;
  }
}

void step() {
  if (!isActive) return;

  uint32_t now     = millis();
  uint32_t elapsed = now - startMs;

  uint16_t* live = matrix.getBuffer();
  memset(live, 0, sizeof(uint16_t) * PIX_COUNT);

  bool anyMoving = false;

  for (int i = 0; i < numDrops; ++i) {
    Drop& d = drops[i];

    int32_t  y_q          = d.y_start_q;
    bool     visible      = true;
    bool     moving       = false;
    bool     locked       = false;
    uint16_t lockedColor  = COLOR_LOCKED;

    if (elapsed < d.start_ms) {
      moving = true;
      if (d.kind == KIND_OUT) {
        // Pre-launch OUT drop is still part of the old time. Render it as
        // a settled green pixel at its original position so the digits stay
        // visible until each pixel converts to rain — and brighten in the
        // last few frames so the moment of conversion has a clear pop.
        y_q    = d.y_start_q;
        locked = true;
        uint32_t until_launch = d.start_ms - elapsed;
        if      (until_launch < OUT_PRE_FLASH_MS) lockedColor = COLOR_FLASH;
        else if (until_launch < OUT_PRE_HEAD_MS)  lockedColor = COLOR_HEAD;
      } else {
        // IN / BG drops haven't entered the panel yet.
        visible = false;
      }
    } else {
      uint32_t dt        = elapsed - d.start_ms;
      int32_t  advance_q = (int32_t)((dt * 16) / d.fall_ms_per_cell);
      y_q = (int32_t)d.y_start_q + advance_q;

      if (d.kind == KIND_IN) {
        if (y_q >= d.y_target_q) {
          y_q    = d.y_target_q;
          locked = true;
          // Time spent landed: starts at 0 the frame the drop reaches its
          // target. Stage white → white-green → green over ~230 ms so the
          // landing reads as a clear punctuating event.
          uint32_t travel_ms =
              ((uint32_t)((int32_t)d.y_target_q - (int32_t)d.y_start_q) *
               (uint32_t)d.fall_ms_per_cell) >> 4;
          uint32_t land_ms   = d.start_ms + travel_ms;
          uint32_t since_lock = (elapsed > land_ms) ? (elapsed - land_ms) : 0;
          if      (since_lock < IN_LAND_FLASH_MS)
            lockedColor = COLOR_FLASH;
          else if (since_lock < IN_LAND_FLASH_MS + IN_LAND_SETTLE_MS)
            lockedColor = COLOR_HEAD;
          // else: COLOR_LOCKED (default)
        } else {
          moving = true;
        }
      } else {
        if (y_q >= d.y_target_q) {
          visible = false;         // off the bottom
        } else {
          moving = true;
        }
      }
    }

    if (moving) anyMoving = true;
    if (!visible) continue;

    int head_y        = (int)(y_q >> 4);
    int dist_traveled = (int)((y_q - (int32_t)d.y_start_q) >> 4);
    drawDrop(live, (int)d.x, head_y, dist_traveled, locked, lockedColor);
  }

  if (!anyMoving) {
    isActive = false;
    // Normalize the final frame to the settled time only, so any in-flight
    // trail we drew this frame doesn't linger after we stop stepping.
    memset(live, 0, sizeof(uint16_t) * PIX_COUNT);
    for (int i = 0; i < PIX_COUNT; ++i) {
      if (mget(inMask, i)) live[i] = COLOR_LOCKED;
    }
  }
}

void renderColored(Adafruit_GFX& g, int hour12, int minute, bool am) {
  constexpr uint16_t BG = 0x0000;

  renderTargetCanvas(hour12, minute, am);

  const uint16_t* tgt = targetCanvas.getBuffer();
  g.fillScreen(BG);
  for (int y = 0; y < M_H; ++y) {
    for (int x = 0; x < M_W; ++x) {
      if (tgt[y * M_W + x] != BG) {
        g.drawPixel(x, y, COLOR_LOCKED);
      }
    }
  }
}

}  // namespace matrix_rain
