#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

namespace clock_view {

void buildTrigLUT();

// Drawing surface is any Adafruit_GFX (live matrix or GFXcanvas16).
// renderAnalog draws to the right half only; leaves left half untouched.
void renderAnalog(Adafruit_GFX& g, int hour12, int minute);

// renderDigital draws a full-screen centered HH:MMAM/PM string.
void renderDigital(Adafruit_GFX& g, int hour12, int minute, bool am, uint16_t textColor);

// Face color (analog tick + ring) live setter. Loaded from NVS at startup.
void setFaceColor(uint16_t c);
uint16_t faceColor();

}  // namespace clock_view
