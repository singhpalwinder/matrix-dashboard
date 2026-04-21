#pragma once
#include <stdint.h>

// Cross-task flags. All are written from exactly one side (server task or
// main loop) and read from the other, so volatile + simple boolean/uint ops
// is sufficient. Do not read-modify-write from both sides.

extern volatile bool     g_needsRedraw;
extern volatile bool     g_turnOff;
extern volatile bool     g_uploadInProgress;
extern volatile bool     g_processingRequest;
extern volatile uint16_t g_textColor;
