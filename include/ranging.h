#pragma once

#include <Arduino.h>

#define RANGING_BIT_DESK_MOVE 1
#define RANGING_BIT_DESK_STATUS 2
#define RANGING_BIT_MQTT 8
#define RANGING_BIT_SERIAL 16

typedef struct ranging_result_t {
    uint16_t value;
    unsigned long time;
    bool valid;
} ranging_result_t;

void rangingSetup();
const ranging_result_t rangingWaitForNewResult(const unsigned long lastTime, unsigned long timeout = 0);
const ranging_result_t rangingWaitForAnyResult(unsigned long timeout = 0);
const ranging_result_t rangingWaitForNextResult(unsigned long timeout = 0);
void rangingAcquireBit(uint8_t bit);
void rangingReleaseBit(uint8_t bit);
