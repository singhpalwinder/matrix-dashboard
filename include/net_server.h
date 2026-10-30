#pragma once

// Starts both servers and pins the serverTask to core 0. Call after either
// a successful STA connection or starting the AP fallback.
void netServerBegin();

// Controls what the portal shows and how endpoints behave (e.g. restart on
// new WiFi creds only makes sense when we're running as an AP).
void netSetApMode(bool isAp);

// Blank/restore the display; server-task only so g_turnOff keeps one writer.
void applySleep(bool off);
