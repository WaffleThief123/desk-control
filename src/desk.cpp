#include <Arduino.h>

#include "desk.h"
#include "config.h"
#include "ranging.h"

static void deskStop() {
    digitalWrite(PIN_RELAY_UP, LOW);
    digitalWrite(PIN_RELAY_DOWN, LOW);
}

void deskSetup() {
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    deskStop();
}

void deskAdjustHeight(int16_t target) {
    rangingStart();
    while (1) {
        const int16_t heightDiff = target - rangingGetDistance();
        if (abs(heightDiff) <= DESK_HEIGHT_TOLERANCE) {
            break;
        }

        if (heightDiff > 0) { // target > current => move up
            digitalWrite(PIN_RELAY_DOWN, LOW);
            digitalWrite(PIN_RELAY_UP, HIGH);
        } else { // target < current => move down
            digitalWrite(PIN_RELAY_UP, LOW);
            digitalWrite(PIN_RELAY_DOWN, HIGH);
        }
    }
    deskStop();
    rangingStop();
}
