#include "prefs_store.h"
#include <Preferences.h>

static Preferences prefs;

void prefsBegin() {
  // Preferences opens per-call; nothing to do up front, but we could validate
  // the namespace by opening read-only once.
  prefs.begin("matrix", true);
  prefs.end();
}

bool loadColor(const char* key, uint16_t& out) {
  prefs.begin("matrix", true);
  bool has = prefs.isKey(key);
  if (has) out = prefs.getUShort(key, out);
  prefs.end();
  return has;
}

void saveColor(const char* key, uint16_t color) {
  prefs.begin("matrix", false);
  prefs.putUShort(key, color);
  prefs.end();
}

bool loadString(const char* key, String& out) {
  prefs.begin("matrix", true);
  bool has = prefs.isKey(key);
  if (has) out = prefs.getString(key, "");
  prefs.end();
  return has;
}

void saveString(const char* key, const String& value) {
  prefs.begin("matrix", false);
  prefs.putString(key, value);
  prefs.end();
}

bool loadU8(const char* key, uint8_t& out) {
  prefs.begin("matrix", true);
  bool has = prefs.isKey(key);
  if (has) out = prefs.getUChar(key, out);
  prefs.end();
  return has;
}

void saveU8(const char* key, uint8_t value) {
  prefs.begin("matrix", false);
  prefs.putUChar(key, value);
  prefs.end();
}
