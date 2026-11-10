#pragma once
#include <WiFi.h>

namespace ota {

enum class Phase : uint8_t { IDLE, ERASING, WRITING, VERIFYING, DONE, FAILED };

// Streams a signed image into the spare OTA slot; returns nullptr on success, else an error.
const char* receive(WiFiClient& c, size_t len, const String& sigB64);

// Progress, for the update screen (read from the render loop).
Phase    phase();
uint32_t received();
uint32_t total();
uint32_t phaseStartedMs();

// The render loop calls this after it has put a phase's screen on the panel.
// Erasing waits for it so the panel is blank while flash erases stall the refresh.
void ackShown(Phase p);

// Leaves the FAILED screen and returns to normal views.
void clearFailed();

}  // namespace ota
