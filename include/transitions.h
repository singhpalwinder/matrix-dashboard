#pragma once
#include <Adafruit_GFX.h>

namespace transitions {

// Render the incoming view into toSurface(), then call start(). The "from"
// side is captured implicitly from the live framebuffer (which already holds
// whatever was last rendered).
Adafruit_GFX& toSurface();

void start();
bool active();
void step();       // writes next batch of pixels into the live framebuffer

}  // namespace transitions
