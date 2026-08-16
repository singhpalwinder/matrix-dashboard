#pragma once
#include <stdint.h>

// Matrix geometry. Use M_W / M_H to avoid colliding with the WIDTH/HEIGHT
// member variables on Adafruit_GFX.
constexpr int M_W       = 64;
constexpr int M_H       = 32;
constexpr int PIX_COUNT = M_W * M_H;

// MatrixPortal S3 pins
extern uint8_t rgbPins[6];
extern uint8_t addrPins[5];
constexpr uint8_t CLOCK_PIN      = 2;
constexpr uint8_t LATCH_PIN      = 47;
constexpr uint8_t OE_PIN         = 14;
constexpr int     NUM_ADDR_PINS  = 4;  // 32-row panel on MatrixPortal S3

// Image/GIF frame format (32x32 RGB565 big-endian on the wire)
constexpr int     FRAME_WIDTH    = 32;
constexpr int     FRAME_HEIGHT   = 32;
constexpr int     FRAME_SIZE     = FRAME_WIDTH * FRAME_HEIGHT * 2;
constexpr int     MAX_FRAMES     = 10;
constexpr int     NET_BUF_SIZE   = 1024;

// Analog clock (drawn on right half in image modes)
constexpr int     CLOCK_CX       = 47;
constexpr int     CLOCK_CY       = 15;
constexpr int     CLOCK_R        = 15;

// Transition
constexpr uint16_t TRANSITION_MS = 300;

// Global brightness scale (0..255). 255 = unchanged. Lower values dim every
// pixel before it lands on the matrix, which both cuts panel current draw
// (helps with PSU sag flicker on busy frames) and softens the look. Adjust
// here if you want it dimmer or brighter.
constexpr uint8_t BRIGHTNESS = 180;  // ~70% — enough headroom for 2.5A supply

// Scale an RGB565 color by BRIGHTNESS. Inline so callers in tight pixel
// loops don't pay a function-call cost.
static inline uint16_t dimRGB565(uint16_t c) {
  uint16_t r = (c >> 11) & 0x1F;
  uint16_t g = (c >> 5)  & 0x3F;
  uint16_t b =  c        & 0x1F;
  r = (r * BRIGHTNESS) >> 8;
  g = (g * BRIGHTNESS) >> 8;
  b = (b * BRIGHTNESS) >> 8;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

// WiFi connect / NTP budgets
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr int      NTP_SYNC_TIMEOUT_S      = 10;

// Timezone
constexpr const char* TIMEZONE   = "America/Chicago";
// Posix TZ string used as an offline fallback when IANA lookup (which needs
// internet) isn't available — e.g. in AP / offline mode.
constexpr const char* TIMEZONE_POSIX = "CST6CDT,M3.2.0,M11.1.0";

// Net ports
constexpr uint16_t HTTP_PORT     = 80;
constexpr uint16_t IMAGE_PORT    = 9090;

// AP fallback — used when we can't join a configured WiFi network, so the
// user can still reach the portal to enter credentials or set the time.
constexpr const char* AP_SSID    = "pauls-matrix-portal";
constexpr const char* AP_PASS    = "";  // empty = open network

// Onboard buttons on the MatrixPortal S3. Active-low with internal pull-up.
// UP cycles to the next clock animation, DOWN cycles back.
constexpr uint8_t  BTN_UP_PIN    = 6;
constexpr uint8_t  BTN_DOWN_PIN  = 7;
