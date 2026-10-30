#include "net_server.h"
#include "mqtt_client.h"
#include "config.h"
#include "app_state.h"
#include "display.h"
#include "image_view.h"
#include "clock_view.h"
#include "prefs_store.h"

#include <Arduino.h>
#include <WiFi.h>
#include <ezTime.h>

static WiFiServer httpServer(HTTP_PORT);
static WiFiServer imageServer(IMAGE_PORT);
static bool g_apMode = false;

void netSetApMode(bool isAp) { g_apMode = isAp; }

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

// Query-string parser that URL-decodes %XX and '+' sequences and stops at the
// first '&' or space — the latter matters because GET requests place query
// args on the same line as the " HTTP/1.1" suffix.
static bool parseQueryString(const String& req, const char* key, String& out) {
  String needle = String(key) + "=";
  int idx = req.indexOf(needle);
  if (idx < 0) return false;
  int start = idx + needle.length();
  int end = start;
  while (end < (int)req.length()) {
    char c = req[end];
    if (c == '&' || c == ' ') break;
    end++;
  }
  String raw = req.substring(start, end);
  out = "";
  for (int i = 0; i < (int)raw.length(); i++) {
    char c = raw[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < (int)raw.length()) {
      char h[3] = { raw[(unsigned)i + 1], raw[(unsigned)i + 2], 0 };
      out += (char)strtol(h, nullptr, 16);
      i += 2;
    } else {
      out += c;
    }
  }
  return true;
}

void applySleep(bool off) {
  if (off && !g_turnOff) {
    matrix.fillScreen(0);
    matrix.show();
    g_turnOff = true;
  } else if (!off && g_turnOff) {
    g_turnOff = false;
    g_needsRedraw = true;
  }
}

static void sendOk(WiFiClient& c) {
  c.print(F("HTTP/1.1 200 OK\r\nConnection: close\r\nContent-Type: text/plain\r\n\r\nOK"));
}

static void sendRedirect(WiFiClient& c, const char* where) {
  c.print(F("HTTP/1.1 303 See Other\r\nLocation: "));
  c.print(where);
  c.print(F("\r\nConnection: close\r\n\r\n"));
}

static void sendPortal(WiFiClient& c) {
  String ip;
  if (g_apMode) ip = WiFi.softAPIP().toString();
  else          ip = WiFi.localIP().toString();

  const char* modeLabel = g_apMode ? "AP mode — no WiFi. Configure below."
                                   : "Connected.";

  c.print(F(
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/html; charset=utf-8\r\n"
    "Connection: close\r\n"
    "\r\n"
    "<!doctype html><html><head>"
    "<meta charset=\"utf-8\">"
    "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
    "<title>pauls-matrix-portal</title>"
    "<style>"
    ":root{color-scheme:light dark;"
    "--bg:#ffffff;--fg:#111111;--mut:#6b7280;"
    "--card:#f4f4f5;--border:#e4e4e7;--accent:#2563eb;--accent-fg:#ffffff;}"
    "@media (prefers-color-scheme:dark){:root{"
    "--bg:#0a0a0a;--fg:#fafafa;--mut:#a1a1aa;"
    "--card:#18181b;--border:#27272a;--accent:#3b82f6;--accent-fg:#ffffff;}}"
    "*{box-sizing:border-box}"
    "body{margin:0;padding:24px;font-family:-apple-system,BlinkMacSystemFont,"
    "\"Segoe UI\",Roboto,sans-serif;background:var(--bg);color:var(--fg);}"
    ".wrap{max-width:480px;margin:0 auto;}"
    "h1{font-size:20px;margin:0 0 2px;}"
    ".sub{color:var(--mut);font-size:13px;margin:0 0 20px;}"
    ".card{background:var(--card);border:1px solid var(--border);"
    "border-radius:10px;padding:16px;margin-bottom:14px;}"
    ".card h2{margin:0 0 12px;font-size:15px;}"
    "label{display:block;font-size:12px;margin:0 0 4px;color:var(--mut);}"
    "input{width:100%;padding:9px 10px;border:1px solid var(--border);"
    "border-radius:6px;background:var(--bg);color:var(--fg);font-size:14px;"
    "margin-bottom:10px;font-family:inherit;}"
    "input:focus{outline:2px solid var(--accent);outline-offset:-1px;}"
    "button{width:100%;padding:10px 14px;border:0;border-radius:6px;"
    "background:var(--accent);color:var(--accent-fg);font-size:14px;"
    "font-weight:500;cursor:pointer;}"
    "button:hover{opacity:.92;}"
    ".mut{color:var(--mut);font-size:12px;margin:6px 0 0;}"
    ".link{color:var(--accent);text-decoration:none;cursor:pointer;}"
    ".link:hover{text-decoration:underline;}"
    "</style></head><body><div class=\"wrap\">"
    "<h1>pauls-matrix-portal</h1>"
    "<p class=\"sub\">"));
  c.print(modeLabel);
  c.print(F(" &middot; IP "));
  c.print(ip);
  c.print(F("</p>"

    "<div class=\"card\"><h2>WiFi</h2>"
    "<form action=\"/wifi\" method=\"get\">"
    "<label>SSID</label><input name=\"ssid\" required autocomplete=\"off\">"
    "<label>Password</label><input name=\"pass\" type=\"password\" autocomplete=\"off\">"
    "<button>Save &amp; restart</button>"
    "</form></div>"

    "<div class=\"card\"><h2>Set time</h2>"
    "<form action=\"/settime\" method=\"get\" id=\"tf\">"
    "<label>Local time</label>"
    "<input type=\"datetime-local\" step=\"1\" id=\"local\" required>"
    "<input type=\"hidden\" name=\"epoch\" id=\"epoch\">"
    "<button type=\"submit\">Set</button>"
    "</form>"
    "<p class=\"mut\"><a class=\"link\" onclick=\"useNow();return false;\" href=\"#\">"
    "Use this device's current time</a></p>"
    "</div>"

    "<div class=\"card\"><h2>Display</h2>"
    "<p class=\"mut\" style=\"margin:0 0 8px\">Sleep</p>"
    "<form action=\"/sleep/off\" method=\"get\" style=\"margin-bottom:8px\">"
    "<button>On</button></form>"
    "<form action=\"/sleep/on\" method=\"get\">"
    "<button style=\"background:var(--card);color:var(--fg);border:1px solid var(--border)\">Off</button>"
    "</form>"
    "</div>"

    "<script>"
    "function pad(n){return String(n).padStart(2,'0');}"
    "function nowISO(){var d=new Date();"
    "return d.getFullYear()+'-'+pad(d.getMonth()+1)+'-'+pad(d.getDate())+"
    "'T'+pad(d.getHours())+':'+pad(d.getMinutes())+':'+pad(d.getSeconds());}"
    "function useNow(){document.getElementById('local').value=nowISO();}"
    "useNow();"
    "document.getElementById('tf').addEventListener('submit',function(){"
    "var v=document.getElementById('local').value;"
    "if(v)document.getElementById('epoch').value=Math.floor(new Date(v).getTime()/1000);"
    "});"
    "</script>"
    "</div></body></html>"));
}

static void handleHttp(WiFiClient& c) {
  g_processingRequest = true;

  String req = c.readStringUntil('\r');
  while (c.connected()) {
    String line = c.readStringUntil('\n');
    if (line.length() <= 2) break;
  }

  bool handled = false;

  if (req.indexOf("GET / ") >= 0 || req.indexOf("GET /index") >= 0) {
    sendPortal(c);
    handled = true;
  } else if (req.indexOf("GET /wifi?") >= 0) {
    String ssid, pass;
    bool hasSsid = parseQueryString(req, "ssid", ssid);
    parseQueryString(req, "pass", pass);
    if (hasSsid && ssid.length() > 0) {
      saveString(KEY_WIFI_SSID, ssid);
      saveString(KEY_WIFI_PASS, pass);
      c.print(F("HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n\r\n"
                "<!doctype html><meta charset=\"utf-8\">"
                "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                "<style>body{font-family:-apple-system,sans-serif;padding:24px;"
                "background:#0a0a0a;color:#fafafa}"
                "@media(prefers-color-scheme:light){body{background:#fff;color:#111}}"
                "</style><p>Saved. Restarting…</p>"));
      c.flush();
      c.stop();
      delay(500);
      ESP.restart();
      return;  // unreachable
    } else {
      sendRedirect(c, "/");
    }
    handled = true;
  } else if (req.indexOf("GET /settime?") >= 0) {
    int epoch = parseQueryInt(req, "epoch");
    if (epoch > 0) {
      setTime((time_t)epoch);
      sendRedirect(c, "/");
    } else {
      sendRedirect(c, "/");
    }
    handled = true;
  } else if (req.indexOf("POST /reset") >= 0) {
    image_view::reset();
    g_needsRedraw = true;
  } else if (req.indexOf("GET /sleep/on") >= 0) {
    applySleep(true);
    if (req.indexOf("Referer:") < 0) sendRedirect(c, "/");
    else sendOk(c);
    handled = true;
  } else if (req.indexOf("GET /sleep/off") >= 0) {
    applySleep(false);
    if (req.indexOf("Referer:") < 0) sendRedirect(c, "/");
    else sendOk(c);
    handled = true;
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

  if (!handled) {
    c.println(F("HTTP/1.1 200 OK\r\nConnection: close\r\n"));
  }
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

    mqttLoop();

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
