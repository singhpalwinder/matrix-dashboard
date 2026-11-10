#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

namespace clock_view {

void buildTrigLUT();

// Drawing surface is any Adafruit_GFX (live matrix or GFXcanvas16).
// renderAnalog draws to the right half only; leaves left half untouched.
void renderAnalog(Adafruit_GFX& g, int hour12, int minute);

// renderDigital clears the surface and draws the clock face (see drawFace).
void renderDigital(Adafruit_GFX& g, int hour12, int minute, bool am, uint16_t textColor);

// Clock face shared by every digital view and minute animation: "h:mm" in the
// 6x11 bitmap digits at 2x (12x22), and the date underneath in the 5x7 font.
// Draws on top of whatever is there; callers clear first. hour12 < 0 draws "--:--".
void drawFace(Adafruit_GFX& g, int hour12, int minute, uint16_t color);

// The two halves of drawFace. Minute animations only move the time; the date is a
// still layer drawn on top of every animation frame.
void drawTime(Adafruit_GFX& g, int hour12, int minute, uint16_t color);
void drawDate(Adafruit_GFX& g, uint16_t color);
// True for pixels that belong to the time in the current layout; animations ignore
// everything else on the panel (date, weather tiles).
bool inTimeArea(int x, int y);

// "h:mm" in the 6x11 digits at 1x, centered in [x0, x1], top row at y.
void drawTimeSmall(Adafruit_GFX& g, int x0, int x1, int y, int hour12, int minute, uint16_t color);

// Date line for drawFace, e.g. "WED NOV 11". Empty hides it.
void setDate(const char* text);

// Compact face: time at 1x in the top-left quarter and the date across the middle,
// leaving the top-right (x 32-63, y 0-12) and bottom rows (y 23-31) for Pi clips.
void setCompact(bool compact);

// Face color (analog tick + ring) live setter. Loaded from NVS at startup.
void setFaceColor(uint16_t c);
uint16_t faceColor();

}  // namespace clock_view
