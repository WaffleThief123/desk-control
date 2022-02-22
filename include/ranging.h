#pragma once

#include <Arduino.h>

void rangingSetup();
void rangingStart();
void rangingStop();
void rangingLoop();
int16_t rangingGetDistance();
int16_t rangingWaitAndGetDistance();
