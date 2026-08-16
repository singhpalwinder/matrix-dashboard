#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// Dispatcher for the digital-clock minute-change animations. Each style
// implements the same `start / active / step / renderColored` quartet; this
// module routes calls to the currently-selected style and handles button
// cycling + persistence.
namespace clock_anim {

enum Style : uint8_t {
  STYLE_TETRIS      = 0,
  STYLE_MARIO       = 1,
  STYLE_MATRIX_RAIN = 2,
  NUM_STYLES        = 3,
};

void   begin();              // load saved style + configure buttons
Style  current();
void   setStyle(Style s);
void   cycle(int8_t dir);    // +1 or -1
void   pollButtons();        // call from main loop
const char* name(Style s);

void startForDigital(int hour12, int minute, bool am, uint16_t textColor);
bool active();
void step();

void renderColored(Adafruit_GFX& g, int hour12, int minute, bool am);

}  // namespace clock_anim
