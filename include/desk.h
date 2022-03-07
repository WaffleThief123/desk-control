#pragma once

#include <Arduino.h>

void deskSetup();
void deskAdjustHeight(int16_t _target);
int8_t deskGetMovingDirection();
void deskStop();
int16_t deskGetTarget();
