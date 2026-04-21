#include "net_server.h"
#include "config.h"
#include "app_state.h"
#include "display.h"
#include "image_view.h"
#include "clock_view.h"
#include "prefs_store.h"

#include <Arduino.h>
#include <WiFi.h>

static WiFiServer httpServer(HTTP_PORT);
static WiFiServer imageServer(IMAGE_PORT);

static int parseQueryInt(const String& req, const char* key) {
  String needle = String(key) + "=";
  int idx = req.indexOf(needle);
  if (idx < 0) return -1;
  int end = req.indexOf("&", idx);
  if (end < 0) end = req.length();
  int start = idx + needle.length();
  if (start >= (int)req.length()) return -1;
  return req.substring(start, end).toInt();
}

static void applySleep(bool off) {
  if (off && !g_turnOff) {
    matrix.fillScreen(0);
    matrix.show();
    g_turnOff = true;
  } else if (!off && g_turnOff) {
    g_turnOff = false;
    g_needsRedraw = true;
  }
}

static void handleHttp(WiFiClient& c) {
  g_processingRequest = true;

  String req = c.readStringUntil('\r');
  while (c.connected()) {
    String line = c.readStringUntil('\n');
    if (line.length() <= 2) break;
  }

  if (req.indexOf("POST /reset") >= 0) {
    image_view::reset();
    g_needsRedraw = true;
  } else if (req.indexOf("GET /sleep/on") >= 0) {
    applySleep(true);
  } else if (req.indexOf("GET /sleep/off") >= 0) {
    applySleep(false);
  } else if (req.indexOf("GET /color?") >= 0) {
    int r = parseQueryInt(req, "r");
    int g = parseQueryInt(req, "g");
    int b = parseQueryInt(req, "b");
    if (r >= 0 && g >= 0 && b >= 0) {
      uint16_t col = matrix.color565((uint8_t)r, (uint8_t)g, (uint8_t)b);
      g_textColor = col;
      saveColor(KEY_TEXT_COLOR, col);
      g_needsRedraw = true;
    }
  } else if (req.indexOf("GET /clock_color?") >= 0) {
    int r = parseQueryInt(req, "r");
    int g = parseQueryInt(req, "g");
    int b = parseQueryInt(req, "b");
    if (r >= 0 && g >= 0 && b >= 0) {
      uint16_t col = matrix.color565((uint8_t)r, (uint8_t)g, (uint8_t)b);
      clock_view::setFaceColor(col);
      saveColor(KEY_FACE_COLOR, col);
      g_needsRedraw = true;
    }
  }

  c.println("HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n");
  c.stop();
  g_processingRequest = false;
}

static void handleImageUpload(WiFiClient& c) {
  g_uploadInProgress = true;
  static uint8_t netBuf[NET_BUF_SIZE];

  image_view::beginUpload();

  while (c.connected()) {
    int avail = c.available();
    if (avail <= 0) {
      delay(1);
      continue;
    }
    while (avail > 0) {
      int chunk = avail > NET_BUF_SIZE ? NET_BUF_SIZE : avail;
      int n = c.read(netBuf, chunk);
      if (n <= 0) break;
      image_view::writeChunk(netBuf, n);
      avail -= n;
    }
  }

  image_view::endUpload();
  g_needsRedraw = true;
  c.stop();
  g_uploadInProgress = false;
}

static void serverTask(void*) {
  for (;;) {
    WiFiClient http = httpServer.available();
    if (http) handleHttp(http);

    WiFiClient img = imageServer.available();
    if (img) handleImageUpload(img);

    vTaskDelay(5 / portTICK_PERIOD_MS);
  }
}

void netServerBegin() {
  httpServer.begin();
  imageServer.begin();

  xTaskCreatePinnedToCore(
    serverTask,
    "ServerTask",
    16384,
    nullptr,
    1,
    nullptr,
    0   // pin to WiFi core
  );
}
