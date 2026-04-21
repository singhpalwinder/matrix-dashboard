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

// WiFi connect / NTP budgets
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr int      NTP_SYNC_TIMEOUT_S      = 10;

// Timezone
constexpr const char* TIMEZONE   = "America/Chicago";

// Net ports
constexpr uint16_t HTTP_PORT     = 80;
constexpr uint16_t IMAGE_PORT    = 9090;
