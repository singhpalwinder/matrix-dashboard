#pragma once
#include <stdint.h>
#include <Arduino.h>

void prefsBegin();

bool loadColor(const char* key, uint16_t& out);
void saveColor(const char* key, uint16_t color);

bool loadString(const char* key, String& out);
// Returns false if the write did not land (NVS full/error, or read-back mismatch).
bool saveString(const char* key, const String& value);

constexpr const char* KEY_TEXT_COLOR = "txtcol";
constexpr const char* KEY_FACE_COLOR = "facecol";
constexpr const char* KEY_WIFI_SSID  = "wifi_ssid";
constexpr const char* KEY_WIFI_PASS  = "wifi_pass";
constexpr const char* KEY_ANIM_STYLE = "anim";

bool loadU8(const char* key, uint8_t& out);
void saveU8(const char* key, uint8_t value);
