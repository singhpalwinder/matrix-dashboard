#include "audio_levels.h"
#include "config.h"

#include <Arduino.h>
#include <WiFiUdp.h>
#include <string.h>

namespace audio_levels {

static WiFiUDP udp;
static bool started = false;

// Written by the server task, read by the render loop. A torn read only mixes two
// consecutive spectra for one frame, which isn't visible.
static volatile uint8_t  levels[MAX_BANDS];
static volatile int      bands        = 0;
static volatile uint32_t lastPacketMs = 0;
static volatile uint32_t lastLoudMs   = 0;

static constexpr uint32_t STALE_MS  = 250;   // no packet for this long: bars drop
static constexpr uint32_t QUIET_MS  = 1500;  // no sound for this long: record stops
static constexpr uint8_t  LOUD_LEVEL = 12;

void begin() {
  started = udp.begin(AUDIO_PORT) == 1;
}

void poll() {
  if (!started) return;
  uint8_t buf[3 + MAX_BANDS];
  while (int size = udp.parsePacket()) {
    int n = udp.read(buf, sizeof(buf));
    if (size > (int)sizeof(buf)) udp.flush();
    if (n < 3 || buf[0] != 'A' || buf[1] != 'L') continue;
    int count = buf[2];
    if (count > MAX_BANDS || n < 3 + count) continue;
    bool loud = false;
    for (int i = 0; i < count; ++i) {
      levels[i] = buf[3 + i];
      if (buf[3 + i] >= LOUD_LEVEL) loud = true;
    }
    bands = count;
    uint32_t now = millis();
    lastPacketMs = now;
    if (loud) lastLoudMs = now;
  }
}

int latest(uint8_t* out, int max, uint32_t nowMs) {
  if (bands == 0 || nowMs - lastPacketMs > STALE_MS) return 0;
  int n = bands < max ? bands : max;
  for (int i = 0; i < n; ++i) out[i] = levels[i];
  return n;
}

bool playing(uint32_t nowMs) {
  return lastLoudMs != 0 && nowMs - lastLoudMs < QUIET_MS;
}

}  // namespace audio_levels
