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
static volatile uint32_t lastElsewhereMs = 0;

static constexpr uint32_t STALE_MS  = 250;   // no packet for this long: bars drop
static constexpr uint32_t QUIET_MS  = 1500;  // no sound for this long: record stops
static constexpr uint8_t  LOUD_LEVEL = 12;
static constexpr uint32_t ELSEWHERE_MS        = 2500;   // the Pi sends 'NP' every second
static constexpr uint32_t ELSEWHERE_LAYOUT_MS = 15000;

// True if `t` is less than `window` ms before `now`. Signed, because the timestamps are
// written by the server task: one can land after the render loop read `now`, and
// unsigned `now - t` would then wrap to ~49 days and read as long ago. That made the
// soundbar layout drop to the bars for a frame whenever an 'NP' packet raced a frame.
static inline bool within(uint32_t now, uint32_t t, uint32_t window) {
  return t != 0 && (int32_t)(now - t) < (int32_t)window;
}

void begin() {
  started = udp.begin(AUDIO_PORT) == 1;
}

void poll() {
  if (!started) return;
  uint8_t buf[3 + MAX_BANDS];
  while (int size = udp.parsePacket()) {
    int n = udp.read(buf, sizeof(buf));
    if (size > (int)sizeof(buf)) udp.flush();
    if (n >= 2 && buf[0] == 'N' && buf[1] == 'P') {
      lastElsewhereMs = millis();
      continue;
    }
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
  if (bands == 0 || !within(nowMs, lastPacketMs, STALE_MS + 1)) return 0;
  int n = bands < max ? bands : max;
  for (int i = 0; i < n; ++i) out[i] = levels[i];
  return n;
}

bool playing(uint32_t nowMs) {
  return within(nowMs, lastLoudMs, QUIET_MS);
}

bool playingElsewhere(uint32_t nowMs) {
  return within(nowMs, lastElsewhereMs, ELSEWHERE_MS);
}

bool elsewhereLayout(uint32_t nowMs) {
  uint32_t np = lastElsewhereMs, al = lastPacketMs;
  if (!within(nowMs, np, ELSEWHERE_LAYOUT_MS)) return false;
  // Levels from the Pi since the last 'NP': the Pi has the music now.
  return al == 0 || (int32_t)(np - al) > 0;
}

}  // namespace audio_levels
