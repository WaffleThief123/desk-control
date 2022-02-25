#pragma once

#include <Arduino.h>

void deskSetup();
void deskAdjustHeight(int16_t _target, const char *_mqttId);
int8_t deskIsMoving();
void deskStop();
int16_t deskGetTarget();
