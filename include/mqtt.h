#pragma once

#include <Arduino.h>

void mqttSetup();
void mqttSend(const char *data, const size_t len);
bool mqttIsConnected();
void mqttSetLastError(String lastError);
void mqttSetDebug(String debugMsg);
void mqttDoHeightUpdate();
void mqttSetStopReason(String stopReason);
void mqttDoAttributeUpdate();
