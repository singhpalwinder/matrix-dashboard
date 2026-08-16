#include "tetris_transition.h"
#include "config.h"
#include "display.h"
#include <Adafruit_GFX.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/Picopixel.h>
#include <Arduino.h>
#include <string.h>

namespace tetris_transition {

static GFXcanvas16 targetCanvas(M_W, M_H);

static constexpr int MASK_BYTES = (PIX_COUNT + 7) / 8;
static uint8_t outMask[MASK_BYTES];   // old text pixels (falling off)
static uint8_t inMask[MASK_BYTES];    // new text pixels (falling in)
static uint8_t claimed[MASK_BYTES];

static inline bool mget(const uint8_t* m, int idx) {
  return (m[idx >> 3] >> (idx & 7)) & 1;
}
static inline void mset(uint8_t* m, int idx) {
  m[idx >> 3] |= (uint8_t)(1u << (idx & 7));
}

// Retro/tetris palette (RGB565). Each tetromino type keeps its traditional
// color so the animation reads as actual tetris blocks.
// Pastel / NES-Mario palette. Colors chosen to be distinct and all roughly
// equal brightness so no piece fades relative to the others. Each piece
// gets a color from a shuffled deck (see nextDeckColor) so neighbors rarely
// share a color.
static const uint16_t PALETTE[] = {
  0xFDB1,  // peach
  0x8F36,  // mint
  0x869F,  // sky blue
  0xCD5F,  // lavender
  0xFD59,  // rose
  0xFF91,  // lemon
  0x86FA,  // turquoise
  0xFC70,  // salmon
  0xAD9F,  // periwinkle
  0xCF32,  // pistachio
  0xE186,  // mario red
  0xF648,  // koopa yellow
};
static constexpr int NUM_PALETTE = sizeof(PALETTE) / sizeof(PALETTE[0]);

// Shape templates. Anchor (first cell) is always (0,0) and all offsets are
// >= 0 so the top-to-bottom / left-to-right scan in buildPieces finds a
// piece's anchor cell before any of its other cells. Color is assigned
// per-piece (not per-shape) from the shuffled palette deck.
struct Shape {
  uint8_t n;
  int8_t  dx[4];
  int8_t  dy[4];
};

static const Shape SHAPES[] = {
  {4, {0,0,0,0}, {0,1,2,3}},   // I vertical 4
  {3, {0,0,0,0}, {0,1,2,0}},   // I vertical 3
  {2, {0,0,0,0}, {0,1,0,0}},   // I vertical 2
  {4, {0,1,2,3}, {0,0,0,0}},   // I horizontal 4
  {3, {0,1,2,0}, {0,0,0,0}},   // I horizontal 3
  {2, {0,1,0,0}, {0,0,0,0}},   // I horizontal 2
  {4, {0,1,0,1}, {0,0,1,1}},   // O
  {4, {0,0,0,1}, {0,1,2,2}},   // L
  {4, {0,1,0,0}, {0,0,1,2}},   // J
  {4, {0,1,2,1}, {0,0,0,1}},   // T
  {4, {0,0,1,1}, {0,1,1,2}},   // S
};
static constexpr int NUM_SHAPES = sizeof(SHAPES) / sizeof(SHAPES[0]);

struct Piece {
  int8_t   anchor_x;
  int8_t   n;
  int8_t   dx[4];
  int8_t   dy[4];
  int16_t  y_start_q;        // Q4 starting y of anchor
  int16_t  y_target_q;       // Q4 ending y of anchor
  uint16_t color;
  uint32_t start_ms;
  uint16_t fall_ms_per_cell; // per-piece so pieces don't fall in lockstep
  bool     fallingOut;
};

static constexpr int MAX_PIECES = 260;
static Piece pieces[MAX_PIECES];
static int   numPieces = 0;

static uint32_t startMs  = 0;
static bool     isActive = false;

// Fall speed and stagger. Each piece picks its own speed within
// [FALL_MS_BASE, FALL_MS_BASE + FALL_MS_JITTER] so the old frame doesn't
// look like a single sliding block, and the stagger window is wide enough
// that a clock-full of pieces spreads out visibly instead of bunching.
static constexpr uint32_t FALL_MS_BASE     = 75;
static constexpr uint32_t FALL_MS_JITTER   = 55;   // speeds in 75..129
static constexpr uint32_t OUT_STAGGER_MS   = 1800;
static constexpr uint32_t IN_DELAY_MS      = 400;
static constexpr uint32_t IN_STAGGER_MS    = 2200;

static uint32_t xs = 0xCAFEBABEu;
static inline uint32_t xs32() {
  uint32_t x = xs;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  xs = x;
  return x;
}

// Shuffled-deck color picker: walk through all palette entries in a random
// order before any color can repeat. The deck is shuffled when exhausted,
// with a swap to prevent the last color of one deck meeting the first color
// of the next.
static uint8_t deck[NUM_PALETTE];
static int     deckIdx      = NUM_PALETTE;  // forces initial shuffle
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

static bool tryFit(const uint8_t* src, int ax, int ay, const Shape& s,
                   Piece& out) {
  for (uint8_t i = 0; i < s.n; ++i) {
    int cx = ax + s.dx[i];
    int cy = ay + s.dy[i];
    if (cx < 0 || cx >= M_W || cy < 0 || cy >= M_H) return false;
    int idx = cy * M_W + cx;
    if (!mget(src, idx)) return false;
    if (mget(claimed, idx)) return false;
  }
  out.n = s.n;
  for (uint8_t i = 0; i < s.n; ++i) {
    out.dx[i] = s.dx[i];
    out.dy[i] = s.dy[i];
    int cx = ax + s.dx[i];
    int cy = ay + s.dy[i];
    mset(claimed, cy * M_W + cx);
  }
  return true;
}

static void buildPieces(const uint8_t* src, bool fallingOut,
                        const uint16_t* colorSrc) {
  memset(claimed, 0, sizeof(claimed));
  for (int y = 0; y < M_H; ++y) {
    for (int x = 0; x < M_W; ++x) {
      int idx = y * M_W + x;
      if (!mget(src, idx)) continue;
      if (mget(claimed, idx)) continue;
      if (numPieces >= MAX_PIECES) return;

      Piece& p = pieces[numPieces];
      p.anchor_x   = (int8_t)x;
      p.fallingOut = fallingOut;

      bool placed = false;
      int start = (int)(xs32() % NUM_SHAPES);
      for (int k = 0; k < NUM_SHAPES; ++k) {
        const Shape& s = SHAPES[(start + k) % NUM_SHAPES];
        if (tryFit(src, x, y, s, p)) { placed = true; break; }
      }
      if (!placed) {
        p.n = 1;
        p.dx[0] = 0;
        p.dy[0] = 0;
        mset(claimed, idx);
      }

      // Falling-out pieces inherit the live pixel's existing color so the old
      // digits don't visibly "shift" into new palette colors during the pre-
      // fall hold. Falling-in pieces still pull fresh colors from the deck.
      if (colorSrc) {
        p.color = colorSrc[idx];
      } else {
        p.color = nextDeckColor();
      }
      p.fall_ms_per_cell =
          (uint16_t)(FALL_MS_BASE + (xs32() % FALL_MS_JITTER));

      int maxDy = 0;
      for (uint8_t i = 0; i < p.n; ++i) {
        if (p.dy[i] > maxDy) maxDy = p.dy[i];
      }

      if (fallingOut) {
        p.y_start_q  = (int16_t)(y << 4);
        p.y_target_q = (int16_t)((M_H + maxDy + 2) << 4);
        p.start_ms   = xs32() % OUT_STAGGER_MS;
      } else {
        int stagger  = (int)(xs32() % 12);
        int above_q  = (maxDy + 2 + stagger) << 4;
        p.y_start_q  = (int16_t)(-above_q);
        p.y_target_q = (int16_t)(y << 4);
        p.start_ms   = IN_DELAY_MS + (xs32() % IN_STAGGER_MS);
      }
      numPieces++;
    }
  }
}

// Render the digital time into targetCanvas. The pixel color used here is
// arbitrary — callers only consume the buffer as a lit/unlit mask.
static void renderTargetCanvas(int hour12, int minute, bool am) {
  targetCanvas.fillScreen(0x0000);
  targetCanvas.setTextWrap(false);
  targetCanvas.setTextColor(0xFFFF);

  // "h:mm" big — regular weight (1px strokes survive 2x2 brick chunking
  // cleaner than bold's 2px strokes did).
  targetCanvas.setFont(&FreeSans12pt7b);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d:%02d", hour12, minute);

  constexpr int RESERVE_RIGHT = 12;  // for AM/PM badge
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

void startForDigital(int hour12, int minute, bool am, uint16_t /*textColor*/) {
  constexpr uint16_t BG = 0x0000;

  renderTargetCanvas(hour12, minute, am);

  // Capture masks: every lit pixel in the live buffer falls off; every lit
  // pixel in the target falls in. No intersection carved out — even shared
  // pixels get recolored, which is the effect the user asked for.
  const uint16_t* live = matrix.getBuffer();
  const uint16_t* tgt  = targetCanvas.getBuffer();
  memset(outMask, 0, sizeof(outMask));
  memset(inMask,  0, sizeof(inMask));
  for (int i = 0; i < PIX_COUNT; ++i) {
    if (live[i] != BG) mset(outMask, i);
    if (tgt[i]  != BG) mset(inMask,  i);
  }

  numPieces = 0;
  xs ^= millis();

  buildPieces(outMask, true,  live);     // out pieces keep their existing colors
  buildPieces(inMask,  false, nullptr);   // in pieces draw from the palette deck

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

  bool anyActive = false;

  for (int i = 0; i < numPieces; ++i) {
    Piece& p = pieces[i];

    int32_t y_q;
    bool    visible = true;
    bool    moving  = false;

    if (elapsed < p.start_ms) {
      y_q = p.y_start_q;
      moving = true;
      // Not-yet-started falling-out pieces sit at their original position so
      // the old digits don't pop out before the animation begins. Not-yet-
      // started falling-in pieces are still offscreen above.
      if (!p.fallingOut) visible = false;
    } else {
      uint32_t dt        = elapsed - p.start_ms;
      int32_t  advance_q = (int32_t)((dt * 16) / p.fall_ms_per_cell);
      y_q = (int32_t)p.y_start_q + advance_q;

      if (p.fallingOut) {
        if (y_q >= p.y_target_q) {
          visible = false;  // fully off the bottom
        } else {
          moving = true;
        }
      } else {
        if (y_q >= p.y_target_q) {
          y_q = p.y_target_q;  // landed; stays visible at final position
        } else {
          moving = true;
        }
      }
    }

    if (moving) anyActive = true;
    if (!visible) continue;

    int y_top = (int)(y_q >> 4);
    for (uint8_t k = 0; k < p.n; ++k) {
      int cx = p.anchor_x + p.dx[k];
      int cy = y_top + p.dy[k];
      if (cx < 0 || cx >= M_W) continue;
      if (cy < 0 || cy >= M_H) continue;
      live[cy * M_W + cx] = p.color;
    }
  }

  if (!anyActive) {
    // Final frame already drawn from the landed falling-in pieces, so the
    // colored digits persist in the live buffer until the next minute tick.
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

  memset(claimed, 0, sizeof(claimed));
  g.fillScreen(BG);

  xs ^= millis();

  // Same chunking as buildPieces, but we draw each piece directly to `g` at
  // its final position instead of recording it for animation.
  for (int y = 0; y < M_H; ++y) {
    for (int x = 0; x < M_W; ++x) {
      int idx = y * M_W + x;
      if (!mget(inMask, idx)) continue;
      if (mget(claimed, idx)) continue;

      Piece p;
      p.anchor_x = (int8_t)x;

      bool placed = false;
      int start = (int)(xs32() % NUM_SHAPES);
      for (int k = 0; k < NUM_SHAPES; ++k) {
        const Shape& s = SHAPES[(start + k) % NUM_SHAPES];
        if (tryFit(inMask, x, y, s, p)) { placed = true; break; }
      }
      if (!placed) {
        p.n = 1;
        p.dx[0] = 0;
        p.dy[0] = 0;
        mset(claimed, idx);
      }

      uint16_t color = nextDeckColor();
      for (uint8_t k = 0; k < p.n; ++k) {
        int cx = x + p.dx[k];
        int cy = y + p.dy[k];
        if (cx < 0 || cx >= M_W || cy < 0 || cy >= M_H) continue;
        g.drawPixel(cx, cy, color);
      }
    }
  }
}

}  // namespace tetris_transition
