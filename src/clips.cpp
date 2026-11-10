#include "clips.h"
#include "config.h"

#include <Arduino.h>
#include <esp32-hal-psram.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace clips {

namespace {

constexpr size_t HEADER    = 19;
constexpr size_t MAX_BYTES = 64 * 1024;  // per slot

struct Slot {
  uint8_t  x = 0, y = 0, w = 0, h = 0, frames = 0;
  uint16_t delayMs = 100;
  uint32_t ttlMs = 0, tag = 0;
  uint32_t seenMs = 0;     // last Pi check-in
  uint32_t startMs = 0;    // when the loop started
  int      shownFrame = -1;
  uint8_t* data = nullptr; // frames * w * h * 2 bytes, big-endian RGB565
};

Slot slots[MAX_SLOTS];
SemaphoreHandle_t lock;
volatile bool changed = false;

inline uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
inline uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

bool readFully(WiFiClient& c, uint8_t* dst, size_t n) {
  size_t got = 0;
  uint32_t last = millis();
  while (got < n) {
    int r = c.read(dst + got, n - got);
    if (r > 0) { got += r; last = millis(); continue; }
    if (!c.connected() || millis() - last > 3000) return false;
    delay(1);
  }
  return true;
}

bool live(const Slot& s, uint32_t now) {
  return s.data && s.frames && (s.ttlMs == 0 || now - s.seenMs < s.ttlMs);
}

}  // namespace

void begin() { lock = xSemaphoreCreateMutex(); }

const char* receive(WiFiClient& c, size_t len) {
  uint8_t hdr[HEADER];
  if (len < HEADER || !readFully(c, hdr, HEADER)) return "short header";
  if (memcmp(hdr, "CLIP", 4) != 0 || hdr[4] != 1) return "bad magic or version";
  uint8_t slot = hdr[5], x = hdr[6], y = hdr[7], w = hdr[8], h = hdr[9], frames = hdr[10];
  if (slot >= MAX_SLOTS) return "bad slot";

  size_t bytes = (size_t)frames * w * h * 2;
  if (len != HEADER + bytes) return "length doesn't match frames * w * h * 2";
  if (frames && (w == 0 || h == 0 || x + w > M_W || y + h > M_H)) return "region off the panel";
  if (bytes > MAX_BYTES) return "clip too large";

  uint8_t* data = nullptr;
  if (bytes) {
    data = (uint8_t*)(psramFound() ? ps_malloc(bytes) : malloc(bytes));
    if (!data) return "out of memory";
    if (!readFully(c, data, bytes)) { free(data); return "connection dropped"; }
  }

  uint8_t* old;
  xSemaphoreTake(lock, portMAX_DELAY);
  Slot& s = slots[slot];
  old = s.data;
  s.x = x; s.y = y; s.w = w; s.h = h; s.frames = frames;
  s.delayMs = le16(hdr + 11) ? le16(hdr + 11) : 100;
  s.ttlMs = (uint32_t)le16(hdr + 13) * 1000;
  s.tag = le32(hdr + 15);
  s.seenMs = s.startMs = millis();
  s.shownFrame = -1;
  s.data = data;
  xSemaphoreGive(lock);
  free(old);
  changed = true;
  return nullptr;
}

String status() {
  String out;
  uint32_t now = millis();
  xSemaphoreTake(lock, portMAX_DELAY);
  for (int i = 0; i < MAX_SLOTS; ++i) {
    slots[i].seenMs = now;
    if (slots[i].data && slots[i].frames) out += String(i) + " " + String(slots[i].tag) + "\n";
  }
  xSemaphoreGive(lock);
  return out;
}

bool anyActive(uint32_t nowMs) {
  for (const Slot& s : slots) if (live(s, nowMs)) return true;
  return false;
}

bool draw(Adafruit_GFX& g, uint32_t nowMs, bool force) {
  bool drew = false;
  if (xSemaphoreTake(lock, 0) != pdTRUE) return false;  // an upload is swapping; next frame
  for (Slot& s : slots) {
    if (!live(s, nowMs)) {
      if (s.data && s.frames && s.shownFrame >= 0) changed = true;  // just expired
      s.shownFrame = -1;
      continue;
    }
    int frame = (int)(((nowMs - s.startMs) / s.delayMs) % s.frames);
    if (frame == s.shownFrame && !force) continue;
    s.shownFrame = frame;
    const uint8_t* p = s.data + (size_t)frame * s.w * s.h * 2;
    for (int yy = 0; yy < s.h; ++yy) {
      for (int xx = 0; xx < s.w; ++xx, p += 2) {
        g.drawPixel(s.x + xx, s.y + yy, dimRGB565((uint16_t)((p[0] << 8) | p[1])));
      }
    }
    drew = true;
  }
  xSemaphoreGive(lock);
  return drew;
}

bool takeChanged() {
  if (!changed) return false;
  changed = false;
  return true;
}

}  // namespace clips
