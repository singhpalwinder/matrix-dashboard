#include "display.h"
#include <Fonts/FreeSans9pt7b.h>

uint8_t rgbPins[6]  = {42, 40, 41, 38, 37, 39};
uint8_t addrPins[5] = {45, 36, 48, 35, 21};

Adafruit_Protomatter matrix(
  M_W, 5, 1, rgbPins, NUM_ADDR_PINS, addrPins,
  CLOCK_PIN, LATCH_PIN, OE_PIN, true
);

void displayBegin() {
  matrix.begin();
  matrix.setTextWrap(false);
  matrix.setFont(&FreeSans9pt7b);
}
