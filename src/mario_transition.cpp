#include "mario_transition.h"
#include "config.h"
#include "display.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/Picopixel.h>
#include <Arduino.h>
#include <string.h>

// Mario brick-style minute-change animation. Mechanically a tetris-style
// straight-down fall (so the start/stop semantics match the tetris path
// exactly), but pieces are 2x2 NES-brick chunks with a brick palette.
namespace mario_transition {

static GFXcanvas16 targetCanvas(M_W, M_H);

static constexpr int MASK_BYTES = (PIX_COUNT + 7) / 8;
static uint8_t outMask[MASK_BYTES];
static uint8_t inMask[MASK_BYTES];
static uint8_t claimed[MASK_BYTES];

static inline bool mget(const uint8_t* m, int idx) {
  return (m[idx >> 3] >> (idx & 7)) & 1;
}
static inline void mset(uint8_t* m, int idx) {
  m[idx >> 3] |= (uint8_t)(1u << (idx & 7));
}

// NES brick / Mario palette (RGB565).
static const uint16_t PALETTE[] = {
  0xC182,  // brick brown
  0xA0E1,  // dark brick
  0xFCC8,  // tan / brick highlight
  0xE186,  // mario red
  0xFC60,  // koopa orange
  0xFFC0,  // coin yellow
};
static constexpr int NUM_PALETTE = sizeof(PALETTE) / sizeof(PALETTE[0]);

struct Brick {
  int8_t   bx;                 // top-left column (even)
  int8_t   by;                 // anchor row at land (even)
  uint8_t  mask;               // bits 0..3 = cells (i,j) lit in 2x2 footprint
  int16_t  y_start_q;          // Q4 starting y of top-left
  int16_t  y_target_q;         // Q4 target y of top-left
  uint16_t color;
  uint32_t start_ms;
  uint16_t fall_ms_per_cell;
  bool     fallingOut;
};

static constexpr int  MAX_BRICKS = 320;
static Brick          bricks[MAX_BRICKS];
static int            numBricks  = 0;

// Match tetris timing so the two animations feel like the same family.
static constexpr uint32_t FALL_MS_BASE   = 75;
static constexpr uint32_t FALL_MS_JITTER = 55;     // 75..129 ms/cell
static constexpr uint32_t OUT_STAGGER_MS = 1800;
static constexpr uint32_t IN_DELAY_MS    = 400;
static constexpr uint32_t IN_STAGGER_MS  = 2200;

static uint32_t startMs  = 0;
static bool     isActive = false;

static uint32_t xs = 0xDEADBEEFu;
static inline uint32_t xs32() {
  uint32_t x = xs;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  xs = x;
  return x;
}

static uint8_t deck[NUM_PALETTE];
static int     deckIdx       = NUM_PALETTE;
static uint8_t deckLastColor = 0xFF;

static void shuffleDeck() {
  for (int i = 0; i < NUM_PALETTE; ++i) deck[i] = (uint8_t)i;
  for (int i = NUM_PALETTE - 1; i > 0; --i) {
    int j = (int)(xs32() % (uint32_t)(i + 1));
    uint8_t t = deck[i]; deck[i] = deck[j]; deck[j] = t;
  }
  if (deck[0] == deckLastColor && NUM_PALETTE > 1) {
    uint8_t t = deck[0]; deck[0] = deck[1]; deck[1] = t;
  }
  deckIdx = 0;
}

static uint16_t nextDeckColor() {
  if (deckIdx >= NUM_PALETTE) shuffleDeck();
  uint8_t c = deck[deckIdx++];
  deckLastColor = c;
  return PALETTE[c];
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

// Group lit cells of `src` into 2x2 brick chunks aligned to even coords.
// Each chunk emits one Brick record; mask records which of the four cells
// were actually lit so we don't paint stray pixels.
//
// colorSrc, when non-null, supplies the brick's color from one of the lit
// cells of the existing live buffer. This is used for falling-out bricks
// so the old digits don't visibly switch to fresh palette colors during
// the pre-fall hold. Falling-in bricks pass nullptr to draw from the deck.
static void buildBricks(const uint8_t* src, bool fallingOut,
                        const uint16_t* colorSrc) {
  for (int by = 0; by < M_H; by += 2) {
    for (int bx = 0; bx < M_W; bx += 2) {
      uint8_t m = 0;
      int sampleIdx = -1;
      for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
          int cx = bx + i, cy = by + j;
          if (cx >= M_W || cy >= M_H) continue;
          if (mget(src, cy * M_W + cx)) {
            m |= (uint8_t)(1u << (j * 2 + i));
            if (sampleIdx < 0) sampleIdx = cy * M_W + cx;
          }
        }
      }
      if (m == 0) continue;
      if (numBricks >= MAX_BRICKS) return;

      Brick& p = bricks[numBricks++];
      p.bx               = (int8_t)bx;
      p.by               = (int8_t)by;
      p.mask             = m;
      p.color            = (colorSrc && sampleIdx >= 0)
                              ? colorSrc[sampleIdx]
                              : nextDeckColor();
      p.fall_ms_per_cell =
          (uint16_t)(FALL_MS_BASE + (xs32() % FALL_MS_JITTER));
      p.fallingOut       = fallingOut;

      if (fallingOut) {
        p.y_start_q  = (int16_t)(by << 4);
        p.y_target_q = (int16_t)((M_H + 4) << 4);   // off-bottom
        p.start_ms   = xs32() % OUT_STAGGER_MS;
      } else {
        int stagger  = (int)(xs32() % 12);
        int above_q  = (4 + stagger) << 4;
        p.y_start_q  = (int16_t)(-above_q);
        p.y_target_q = (int16_t)(by << 4);
        p.start_ms   = IN_DELAY_MS + (xs32() % IN_STAGGER_MS);
      }
    }
  }
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

  numBricks = 0;
  xs ^= millis();
  shuffleDeck();
  buildBricks(outMask, true,  live);     // out bricks keep their existing colors
  buildBricks(inMask,  false, nullptr);   // in bricks draw from the palette deck

  startMs  = millis();
  isActive = true;
}

bool active() { return isActive; }

void step() {
  if (!isActive) return;

  uint32_t now     = millis();
  uint32_t elapsed = now - startMs;

  uint16_t* live = matrix.getBuffer();
  memset(live, 0, sizeof(uint16_t) * PIX_COUNT);

  bool anyMoving = false;

  for (int i = 0; i < numBricks; ++i) {
    Brick& p = bricks[i];

    int32_t y_q;
    bool    visible = true;
    bool    moving  = false;

    if (elapsed < p.start_ms) {
      y_q = p.y_start_q;
      moving = true;
      // Falling-out bricks sit at their original spot until launch so the
      // old time doesn't pop out before the animation begins. Falling-in
      // bricks are still off the top of the screen.
      if (!p.fallingOut) visible = false;
    } else {
      uint32_t dt        = elapsed - p.start_ms;
      int32_t  advance_q = (int32_t)((dt * 16) / p.fall_ms_per_cell);
      y_q = (int32_t)p.y_start_q + advance_q;

      if (p.fallingOut) {
        if (y_q >= p.y_target_q) {
          visible = false;   // off the bottom
        } else {
          moving = true;
        }
      } else {
        if (y_q >= p.y_target_q) {
          y_q = p.y_target_q;  // landed; stays put
        } else {
          moving = true;
        }
      }
    }

    if (moving) anyMoving = true;
    if (!visible) continue;

    int y_top = (int)(y_q >> 4);
    for (uint8_t k = 0; k < 4; ++k) {
      if (!(p.mask & (1u << k))) continue;
      int cx = p.bx    + (int)(k & 1);
      int cy = y_top   + (int)(k >> 1);
      if (cx < 0 || cx >= M_W || cy < 0 || cy >= M_H) continue;
      live[cy * M_W + cx] = p.color;
    }
  }

  if (!anyMoving) {
    isActive = false;
  }
}

void renderColored(Adafruit_GFX& g, int hour12, int minute, bool am) {
  constexpr uint16_t BG = 0x0000;

  renderTargetCanvas(hour12, minute, am);

  const uint16_t* tgt = targetCanvas.getBuffer();
  memset(inMask, 0, sizeof(inMask));
  for (int i = 0; i < PIX_COUNT; ++i) {
    if (tgt[i] != BG) mset(inMask, i);
  }

  xs ^= millis();
  shuffleDeck();
  g.fillScreen(BG);

  // Same 2x2 chunking as the animated path so the static frame matches the
  // post-animation look exactly.
  for (int by = 0; by < M_H; by += 2) {
    for (int bx = 0; bx < M_W; bx += 2) {
      uint8_t m = 0;
      for (int j = 0; j < 2; ++j) {
        for (int i = 0; i < 2; ++i) {
          int cx = bx + i, cy = by + j;
          if (cx >= M_W || cy >= M_H) continue;
          if (mget(inMask, cy * M_W + cx)) m |= (uint8_t)(1u << (j * 2 + i));
        }
      }
      if (m == 0) continue;
      uint16_t color = nextDeckColor();
      for (uint8_t k = 0; k < 4; ++k) {
        if (!(m & (1u << k))) continue;
        int cx = bx + (int)(k & 1);
        int cy = by + (int)(k >> 1);
        if (cx < 0 || cx >= M_W || cy < 0 || cy >= M_H) continue;
        g.drawPixel(cx, cy, color);
      }
    }
  }
}

}  // namespace mario_transition
