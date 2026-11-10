#pragma once
#include <Adafruit_GFX.h>
#include <WiFi.h>
#include <stdint.h>

// Looping clips the Pi draws for a rectangle of the idle clock screen (e.g. the
// weather tile). The board stores and loops them; it knows nothing about what's in them.
//
// POST /clip body:
//   "CLIP" u8 version(1) u8 slot u8 x u8 y u8 w u8 h u8 frames u16le delay_ms
//   u16le ttl_s u32le tag, then frames * w * h big-endian RGB565 pixels.
//   frames = 0 clears the slot.
// GET /clips lists "slot tag" lines so the Pi can re-push whatever the board lost
// (e.g. after a reboot). Each GET also counts as the Pi checking in: a slot is
// dropped when the Pi hasn't checked in for ttl_s seconds.
namespace clips {

constexpr int MAX_SLOTS = 4;

void begin();

// Server task: handle a POST /clip body of `len` bytes. nullptr on success.
const char* receive(WiFiClient& c, size_t len);
// Server task: GET /clips body; also refreshes every slot's ttl.
String status();

// Render loop.
bool anyActive(uint32_t nowMs);
// Draws the current frame of every active slot. Returns true if any slot showed a
// new frame since the last call, i.e. the panel needs a show().
bool draw(Adafruit_GFX& g, uint32_t nowMs, bool force);
// True once after slots appear, change or disappear (the clock face relays out).
bool takeChanged();

}  // namespace clips
