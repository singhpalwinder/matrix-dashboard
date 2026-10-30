#pragma once

// HA MQTT link: auto-discovery switch entity kept in sync with g_turnOff.
// Call mqttBegin() once after STA connect; mqttLoop() from the server task.
void mqttBegin();
void mqttLoop();
