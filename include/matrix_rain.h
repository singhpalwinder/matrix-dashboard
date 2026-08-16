#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// "The Matrix" style minute-change animation.
//
// On startForDigital, columns of bright-green rain heads sweep down from
// the top of the panel. As each head passes a row, it leaves the new time
// in matrix green where the digit was lit, and black elsewhere. Once every
// column's head has reached the bottom (plus the trail length), the
// animation ends with the new time displayed in steady green.
//
// renderColored paints the time statically in the same green so the boot
// frame and post-animation frame match.
namespace matrix_rain {

void startForDigital(int hour12, int minute, bool am, uint16_t textColor);
bool active();
void step();

void renderColored(Adafruit_GFX& g, int hour12, int minute, bool am);

}  // namespace matrix_rain
