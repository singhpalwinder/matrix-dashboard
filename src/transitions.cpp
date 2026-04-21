#include "transitions.h"
#include "config.h"
#include "display.h"
#include <Arduino.h>

namespace transitions {

static GFXcanvas16 toCanvas(M_W, M_H);

// Scrambled order of pixel indices 0..PIX_COUNT-1. Shuffled on each start().
static uint16_t order[PIX_COUNT];

static uint16_t revealed = 0;
static uint32_t startMs  = 0;
static bool     isActive = false;

Adafruit_GFX& toSurface() { return toCanvas; }

static uint32_t xsState = 0x12345678u;
static inline uint32_t xs32() {
  uint32_t x = xsState;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  xsState = x;
  return x;
}

static void shuffleOrder() {
  for (uint16_t i = 0; i < PIX_COUNT; i++) order[i] = i;
  for (uint16_t i = PIX_COUNT - 1; i > 0; i--) {
    uint16_t j = xs32() % (i + 1);
    uint16_t t = order[i];
    order[i] = order[j];
    order[j] = t;
  }
}

void start() {
  xsState ^= (uint32_t)millis();   // re-seed so successive transitions differ
  shuffleOrder();
  revealed = 0;
  startMs  = millis();
  isActive = true;
}

bool active() { return isActive; }

void step() {
  if (!isActive) return;

  uint32_t elapsed = millis() - startMs;
  uint32_t progress = (elapsed >= TRANSITION_MS)
    ? PIX_COUNT
    : (elapsed * PIX_COUNT) / TRANSITION_MS;

  uint16_t* live = matrix.getBuffer();
  const uint16_t* to = toCanvas.getBuffer();

  for (uint32_t i = revealed; i < progress; i++) {
    uint16_t idx = order[i];
    live[idx] = to[idx];
  }
  revealed = (uint16_t)progress;

  if (progress >= PIX_COUNT) isActive = false;
}

}  // namespace transitions
