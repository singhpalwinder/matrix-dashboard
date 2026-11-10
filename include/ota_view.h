#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// Full-screen firmware update screen: meshing gears from flash, a percentage and a
// progress bar. Takes over the panel for the whole update.
namespace ota_view {

bool active();
// Draws the current phase. Returns the phase drawn, to pass to ota::ackShown after show().
uint8_t render(Adafruit_GFX& g, uint32_t nowMs);

}  // namespace ota_view
