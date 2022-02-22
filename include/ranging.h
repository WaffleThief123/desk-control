#pragma once

#include <Arduino.h>

void rangingSetup();
void rangingStart();
void rangingStop();
int16_t rangingGetDistance();
int16_t rangingWaitAndGetDistance();
