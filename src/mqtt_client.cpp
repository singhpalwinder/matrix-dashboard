#include "mqtt_client.h"
#include "secrets.h"
#include "app_state.h"
#include "net_server.h"

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// Topics: "pauls-matrix" namespace + HA's homeassistant/<comp>/<node>/<obj>/config.
static const char* TOPIC_AVAIL     = "pauls-matrix/status";
static const char* TOPIC_CMD       = "pauls-matrix/display/set";
static const char* TOPIC_STATE     = "pauls-matrix/display/state";
static const char* TOPIC_DISCOVERY =
    "homeassistant/switch/pauls_matrix/display/config";

// Kept long so blocking connect attempts don't hog the shared server task.
static const uint32_t RECONNECT_INTERVAL_MS = 15000;

static WiFiClient   mqttSock;
static PubSubClient mqtt(mqttSock);
static bool         begun           = false;
static uint32_t     lastAttemptMs   = 0;
static int8_t       lastPublishedOn = -1;  // -1 = never published, else 0/1

static void publishState(bool on) {
  mqtt.publish(TOPIC_STATE, on ? "ON" : "OFF", true);
  lastPublishedOn = on ? 1 : 0;
}

static void onMessage(char* topic, byte* payload, unsigned int len) {
  if (strcmp(topic, TOPIC_CMD) != 0) return;
  bool on  = (len == 2 && strncmp((const char*)payload, "ON", 2) == 0);
  bool off = (len == 3 && strncmp((const char*)payload, "OFF", 3) == 0);
  if (!on && !off) return;
  applySleep(off);
  publishState(on);
}

static bool connectBroker() {
  const char* user = MQTT_USER[0] ? MQTT_USER : nullptr;
  const char* pass = MQTT_USER[0] ? MQTT_PASS : nullptr;
  // LWT marks the switch unavailable in HA if we drop off the network.
  if (!mqtt.connect("pauls-matrix", user, pass,
                    TOPIC_AVAIL, 0, true, "offline")) {
    return false;
  }

  mqtt.publish(TOPIC_AVAIL, "online", true);
  // Retained discovery config; HA rebuilds the entity from it after restarts.
  mqtt.publish(TOPIC_DISCOVERY,
               "{\"name\":\"Display\","
               "\"uniq_id\":\"pauls_matrix_display\","
               "\"cmd_t\":\"pauls-matrix/display/set\","
               "\"stat_t\":\"pauls-matrix/display/state\","
               "\"avty_t\":\"pauls-matrix/status\","
               "\"pl_on\":\"ON\",\"pl_off\":\"OFF\","
               "\"ic\":\"mdi:clock-digital\","
               "\"dev\":{\"ids\":[\"pauls_matrix\"],"
               "\"name\":\"Matrix Dashboard\","
               "\"mdl\":\"MatrixPortal S3\",\"mf\":\"Adafruit\"}}",
               true);
  mqtt.subscribe(TOPIC_CMD);
  publishState(!g_turnOff);
  return true;
}

void mqttBegin() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  // Default 256-byte buffer silently drops the discovery payload.
  mqtt.setBufferSize(768);
  mqtt.setSocketTimeout(2);
  mqtt.setCallback(onMessage);
  begun = true;
}

void mqttLoop() {
  if (!begun) return;

  if (!mqtt.connected()) {
    uint32_t now = millis();
    if (lastAttemptMs != 0 && (now - lastAttemptMs) < RECONNECT_INTERVAL_MS) {
      return;
    }
    lastAttemptMs = now;
    if (!connectBroker()) return;
  }

  mqtt.loop();

  // Mirror portal /sleep toggles into MQTT so the HA switch never goes stale.
  int8_t on = g_turnOff ? 0 : 1;
  if (on != lastPublishedOn) publishState(on == 1);
}
