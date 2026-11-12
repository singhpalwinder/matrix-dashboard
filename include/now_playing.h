#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

// Now-playing view: the cover spins as a record on the left while music plays,
// and the right half shows the time over spectrum bars streamed from the Pi. When the
// music is on the soundbar instead (no audio reaches the Pi, so no bars), the record
// still spins and the date sits under the time.
namespace now_playing {

// Draws one frame. Call every frame (~30 fps) while a static cover is showing.
void render(Adafruit_GFX& g, int hour12, int minute, uint32_t nowMs);

}  // namespace now_playing
