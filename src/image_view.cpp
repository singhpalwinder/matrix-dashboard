#include "image_view.h"
#include "config.h"
#include <string.h>

namespace image_view {

static uint8_t  gifFrames[MAX_FRAMES][FRAME_SIZE];
static uint8_t  cachedImage[FRAME_SIZE];

static volatile uint8_t  totalFrames       = 0;
static          uint8_t  currentFrameIndex = 0;
static          uint16_t frameDelayMs      = 100;
static          uint32_t lastFrameSwitchMs = 0;

static volatile bool gifLoaded   = false;
static volatile bool imageLoaded = false;

// Upload accounting
static int      uploadFrame = 0;
static int      uploadPos   = 0;
static uint32_t uploadBytes = 0;

void beginUpload() {
  uploadFrame = 0;
  uploadPos   = 0;
  uploadBytes = 0;
}

void writeChunk(const uint8_t* data, int n) {
  for (int i = 0; i < n; i++) {
    if (uploadFrame >= MAX_FRAMES) return;  // overflow: drop remainder silently
    gifFrames[uploadFrame][uploadPos++] = data[i];
    if (uploadPos >= FRAME_SIZE) {
      uploadPos = 0;
      uploadFrame++;
    }
    uploadBytes++;
  }
}

void endUpload() {
  uint32_t idx = uploadBytes;
  uint8_t frames = (uint8_t)(idx / FRAME_SIZE);
  if (frames > MAX_FRAMES) frames = MAX_FRAMES;

  if (frames > 1) {
    totalFrames = frames;
    currentFrameIndex = 0;
    lastFrameSwitchMs = 0;
    gifLoaded   = true;
    imageLoaded = false;
  } else if (frames == 1 || idx == FRAME_SIZE) {
    memcpy(cachedImage, gifFrames[0], FRAME_SIZE);
    totalFrames = 1;
    imageLoaded = true;
    gifLoaded   = false;
  }
}

bool hasImage() { return imageLoaded || gifLoaded; }
bool hasGif()   { return gifLoaded; }

void reset() {
  imageLoaded = false;
  gifLoaded   = false;
  totalFrames = 0;
  currentFrameIndex = 0;
}

bool tickFrameAdvance(uint32_t nowMs) {
  if (!gifLoaded || totalFrames == 0) return false;
  if ((nowMs - lastFrameSwitchMs) < frameDelayMs) return false;
  currentFrameIndex = (currentFrameIndex + 1) % totalFrames;
  lastFrameSwitchMs = nowMs;
  return true;
}

void renderImage(Adafruit_GFX& g) {
  // Always paint the left 32x32 region so callers get a known state.
  g.fillRect(0, 0, 32, 32, 0x0000);
  if (!(imageLoaded || gifLoaded)) return;

  const uint8_t* frame = gifLoaded ? gifFrames[currentFrameIndex] : cachedImage;
  int idx = 0;
  for (int y = 0; y < FRAME_HEIGHT; y++) {
    for (int x = 0; x < FRAME_WIDTH; x++) {
      uint8_t hi = frame[idx++];
      uint8_t lo = frame[idx++];
      g.drawPixel(x, y, (uint16_t)((hi << 8) | lo));
    }
  }
}

}  // namespace image_view
