#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// Tetris-style rendering and animation for the digital clock.
//
// startForDigital kicks off a minute-change animation where old text pixels
// fall off the bottom and new text pixels fall in from the top, each as a
// retro-colored tetromino piece.
//
// renderColored paints the same pixel-to-piece coloring statically (no fall)
// so the very first on-boot render already matches the retro palette and the
// user doesn't see a white-to-color transition.
namespace tetris_transition {

void startForDigital(int hour12, int minute, bool am, uint16_t textColor);
bool active();
void step();

void renderColored(Adafruit_GFX& g, int hour12, int minute, bool am);

}  // namespace tetris_transition
