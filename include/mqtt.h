#pragma once

#include <Arduino.h>

void mqttSetup();
void mqttLoop();
void mqttSend(const char* data, const size_t len);
void mqttSendJSON(const char* mqttId, const char* type, const char* data);
