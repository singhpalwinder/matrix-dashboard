#pragma once
#include <stdint.h>

// Spectrum bars streamed from the Pi over UDP (AUDIO_PORT). Packet: 'A' 'L' n level[n],
// levels 0..255, ~30 per second while music plays and nothing while it's silent.
//
// 'N' 'P' (about once a second): music is playing, but not through the Pi (it's on the
// Bose soundbar), so there are no levels. The record still spins and the date takes
// the bars' place.
namespace audio_levels {

constexpr int MAX_BANDS = 16;

void begin();  // after WiFi is up
void poll();   // drain pending packets; called from the server task

// Copies the latest levels into out (up to max) and returns how many bands there
// are. Returns 0 when nothing has arrived for a moment, so bars fall to zero.
int latest(uint8_t* out, int max, uint32_t nowMs);

// True while music has been heard recently; drives the record's spin.
bool playing(uint32_t nowMs);

// True while 'N' 'P' packets keep arriving: playing somewhere the Pi can't hear.
bool playingElsewhere(uint32_t nowMs);

// True when the current music is (or just was) playing elsewhere rather than through
// the Pi. Outlasts playingElsewhere() so a pause doesn't flip the layout to empty bars
// in the seconds before the Pi clears the cover.
bool elsewhereLayout(uint32_t nowMs);

}  // namespace audio_levels
