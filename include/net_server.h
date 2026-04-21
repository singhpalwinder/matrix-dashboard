#pragma once

// Starts both servers and pins the serverTask to core 0. Call after WiFi
// is connected.
void netServerBegin();
