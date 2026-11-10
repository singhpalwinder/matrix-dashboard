#pragma once
#include <stdint.h>

// Spectrum bars streamed from the Pi over UDP (AUDIO_PORT). Packet: 'A' 'L' n level[n],
// levels 0..255, ~30 per second while music plays and nothing while it's silent.
namespace audio_levels {

constexpr int MAX_BANDS = 16;

void begin();  // after WiFi is up
void poll();   // drain pending packets; called from the server task

// Copies the latest levels into out (up to max) and returns how many bands there
// are. Returns 0 when nothing has arrived for a moment, so bars fall to zero.
int latest(uint8_t* out, int max, uint32_t nowMs);

// True while music has been heard recently; drives the record's spin.
bool playing(uint32_t nowMs);

}  // namespace audio_levels
