#pragma once
#include <WiFi.h>

namespace ota {

// Streams a signed image into the spare OTA slot; returns nullptr on success, else an error.
const char* receive(WiFiClient& c, size_t len, const String& sigB64);

}  // namespace ota
