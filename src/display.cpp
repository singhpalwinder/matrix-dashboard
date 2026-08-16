#include "display.h"
#include <Fonts/FreeSans12pt7b.h>

uint8_t rgbPins[6]  = {42, 41, 40, 38, 39, 37};
uint8_t addrPins[5] = {45, 36, 48, 35, 21};

Adafruit_Protomatter matrix(
  M_W, 5, 1, rgbPins, NUM_ADDR_PINS, addrPins,
  CLOCK_PIN, LATCH_PIN, OE_PIN, true
);

void displayBegin() {
  // begin() can silently fall back to single-buffer if memory is tight,
  // which manifests as flicker since draws become immediately visible.
  // Surface a non-OK status so it's diagnosable from serial.
  ProtomatterStatus st = matrix.begin();
  if (st != PROTOMATTER_OK) {
    Serial.print("Protomatter begin failed, status=");
    Serial.println((int)st);
  }
  matrix.setTextWrap(false);
  matrix.setFont(&FreeSans12pt7b);
}
