#pragma once

#include <Arduino.h>

typedef struct ranging_result_t {
    int16_t value;
    unsigned long time;
    bool valid;
} ranging_result_t;

void rangingSetup();
const ranging_result_t rangingGetResult();
const ranging_result_t rangingWaitForNewResult();
const ranging_result_t rangingWaitForAnyResult();
