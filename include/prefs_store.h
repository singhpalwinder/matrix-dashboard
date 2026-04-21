#pragma once
#include <stdint.h>

void prefsBegin();

bool loadColor(const char* key, uint16_t& out);
void saveColor(const char* key, uint16_t color);

constexpr const char* KEY_TEXT_COLOR = "txtcol";
constexpr const char* KEY_FACE_COLOR = "facecol";
