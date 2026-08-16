#include "clock_anim.h"
#include "config.h"
#include "app_state.h"
#include "prefs_store.h"
#include "tetris_transition.h"
#include "mario_transition.h"
#include "matrix_rain.h"

#include <Arduino.h>

namespace clock_anim {

static Style g_style = STYLE_TETRIS;

// Debounced button-edge tracker for one pin.
struct Btn {
  uint8_t  pin;
  uint8_t  lastRaw;     // pull-up: 1 = released, 0 = pressed
  uint8_t  stable;
  uint32_t lastChangeMs;
};
static Btn btnUp;
static Btn btnDown;

static constexpr uint32_t DEBOUNCE_MS = 25;

static void btnInit(Btn& b, uint8_t pin) {
  b.pin = pin;
  pinMode(pin, INPUT_PULLUP);
  b.lastRaw      = (uint8_t)digitalRead(pin);
  b.stable       = b.lastRaw;
  b.lastChangeMs = 0;
}

// Returns true exactly once on a clean press (high → low transition).
static bool btnPressed(Btn& b) {
  uint8_t raw = (uint8_t)digitalRead(b.pin);
  if (raw != b.lastRaw) {
    b.lastRaw      = raw;
    b.lastChangeMs = millis();
    return false;
  }
  if (raw != b.stable && (millis() - b.lastChangeMs) > DEBOUNCE_MS) {
    bool press = (b.stable == 1 && raw == 0);
    b.stable = raw;
    return press;
  }
  return false;
}

void begin() {
  uint8_t saved = (uint8_t)STYLE_TETRIS;
  if (loadU8(KEY_ANIM_STYLE, saved) && saved < NUM_STYLES) {
    g_style = (Style)saved;
  } else {
    g_style = STYLE_TETRIS;
  }
  btnInit(btnUp,   BTN_UP_PIN);
  btnInit(btnDown, BTN_DOWN_PIN);
}

Style current() { return g_style; }

void setStyle(Style s) {
  if (s >= NUM_STYLES) return;
  if (s == g_style) return;
  g_style = s;
  saveU8(KEY_ANIM_STYLE, (uint8_t)g_style);
  // Force the next render to be a clean static draw in the new style instead
  // of inheriting whatever pixels the previous style left in the framebuffer.
  g_needsRedraw = true;
}

void cycle(int8_t dir) {
  int s = (int)g_style + (int)dir;
  if (s < 0) s = NUM_STYLES - 1;
  if (s >= (int)NUM_STYLES) s = 0;
  setStyle((Style)s);
}

void pollButtons() {
  if (btnPressed(btnUp))   cycle(+1);
  if (btnPressed(btnDown)) cycle(-1);
}

const char* name(Style s) {
  switch (s) {
    case STYLE_TETRIS:      return "tetris";
    case STYLE_MARIO:       return "mario";
    case STYLE_MATRIX_RAIN: return "matrix";
    default:                return "?";
  }
}

void startForDigital(int hour12, int minute, bool am, uint16_t textColor) {
  switch (g_style) {
    case STYLE_TETRIS:
      tetris_transition::startForDigital(hour12, minute, am, textColor); break;
    case STYLE_MARIO:
      mario_transition::startForDigital(hour12, minute, am, textColor);  break;
    case STYLE_MATRIX_RAIN:
      matrix_rain::startForDigital(hour12, minute, am, textColor);       break;
    default: break;
  }
}

bool active() {
  switch (g_style) {
    case STYLE_TETRIS:      return tetris_transition::active();
    case STYLE_MARIO:       return mario_transition::active();
    case STYLE_MATRIX_RAIN: return matrix_rain::active();
    default:                return false;
  }
}

void step() {
  switch (g_style) {
    case STYLE_TETRIS:      tetris_transition::step(); break;
    case STYLE_MARIO:       mario_transition::step();  break;
    case STYLE_MATRIX_RAIN: matrix_rain::step();       break;
    default: break;
  }
}

void renderColored(Adafruit_GFX& g, int hour12, int minute, bool am) {
  switch (g_style) {
    case STYLE_TETRIS:
      tetris_transition::renderColored(g, hour12, minute, am); break;
    case STYLE_MARIO:
      mario_transition::renderColored(g, hour12, minute, am);  break;
    case STYLE_MATRIX_RAIN:
      matrix_rain::renderColored(g, hour12, minute, am);       break;
    default: break;
  }
}

}  // namespace clock_anim
