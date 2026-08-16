#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// NES-Mario styled minute-change animation.
//
// Out: lit old-frame pixels are grouped into 2x2 "brick" chunks that fly
// outward with random initial upward+sideways velocity and parabolic gravity,
// like Mario punching a brick from below.
//
// In: lit new-frame pixels are grouped into 2x2 brick chunks that drop in
// from above and settle at their target, tinted with an NES brick palette.
//
// renderColored paints the target time statically using the same palette so
// the very first on-boot frame already matches the animated look.
namespace mario_transition {

void startForDigital(int hour12, int minute, bool am, uint16_t textColor);
bool active();
void step();

void renderColored(Adafruit_GFX& g, int hour12, int minute, bool am);

}  // namespace mario_transition
