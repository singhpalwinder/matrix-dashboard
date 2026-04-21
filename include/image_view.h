#pragma once
#include <Adafruit_GFX.h>
#include <stdint.h>

namespace image_view {

// Upload protocol: called from the server task.
void beginUpload();
void writeChunk(const uint8_t* data, int n);
void endUpload();

// Render the image/GIF onto the left 32x32 of the given surface.
// Safe to call regardless of mode; paints background if nothing is loaded.
void renderImage(Adafruit_GFX& g);

// Advance the GIF frame if it's time. Returns true if the frame changed.
bool tickFrameAdvance(uint32_t nowMs);

// State queries.
bool hasImage();   // static or GIF content is loaded
bool hasGif();

void reset();      // clears both static image and GIF state

}  // namespace image_view
