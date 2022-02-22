#include <Arduino.h>

#include "desk.h"
#include "config.h"
#include "ranging.h"

static int16_t startDistance;
static unsigned long timeout;
static unsigned long startTime;
static bool goingUp;
static int failedSpeedTries;
static int16_t speedLastDistance;
static unsigned long speedLastTime;
static unsigned long rangingLastTime;
static int16_t target;
static bool deskMoving;

static void deskStop() {
    deskMoving = false;
    digitalWrite(PIN_RELAY_UP, LOW);
    digitalWrite(PIN_RELAY_DOWN, LOW);
}

void deskSetup() {
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    deskStop();
}

void deskAdjustHeight(int16_t _target) {
    target = _target;
    startTime = millis();

    rangingStart();
    startDistance = rangingWaitAndGetDistance();

    timeout = abs(target - startDistance) * DESK_ADJUST_TIMEOUT_PER_MM;

    Serial.print("Adjusting desk height to ");
    Serial.print(target);
    Serial.print(" from ");
    Serial.print(startDistance);
    Serial.print(" with timeout ");
    Serial.println(timeout);

    if (abs(target - startDistance) < DESK_HEIGHT_TOLERANCE) {
        deskStop();
        rangingStop();
        Serial.println("Desk already at desired height!");
        return;
    }

    goingUp = target > startDistance;
    if (goingUp) {
        digitalWrite(PIN_RELAY_DOWN, LOW);
        digitalWrite(PIN_RELAY_UP, HIGH);
    } else {
        digitalWrite(PIN_RELAY_UP, LOW);
        digitalWrite(PIN_RELAY_DOWN, HIGH);
    }

    failedSpeedTries = 0;
    speedLastDistance = startDistance;
    speedLastTime = startTime;
    rangingLastTime = startTime;
    deskMoving = true;
}

static void deskMoveEnd(const String& reason) {
    deskStop();

    Serial.print("Desk height adjusted to ");
    Serial.print(rangingWaitAndGetDistance());
    Serial.print(" within ");
    Serial.print(millis() - startTime);
    Serial.print(" (");
    Serial.print(reason);
    Serial.println(")");

    rangingStop();
}

void deskLoop() {
    if (!deskMoving) {
        return;
    }

    const unsigned long time = millis();
    if (time - startTime > timeout) {
        deskMoveEnd("MAIN TIMEOUT");
        return;
    }

    const int16_t distance = rangingGetDistance();
    if (distance < 0) {
        if (time - rangingLastTime > DESK_RANGING_TIMEOUT) {
            deskMoveEnd("RANGING TIMEOUT");
        }
        return;
    }
    rangingLastTime = time;

    if (time - speedLastTime >= DESK_CALCULATE_SPEED_TIME) {
        const double speed = abs((double)(distance - speedLastDistance) / (double)(time - speedLastTime));
        speedLastDistance = distance;
        speedLastTime = time;

        if (speed < DESK_SPEED_MIN) {
            failedSpeedTries++;
            if (failedSpeedTries >= DESK_SPEED_TRIES) {
                deskMoveEnd("SPEED TO LOW");
                return;
            }
        } else {
            failedSpeedTries = 0;
        }
    }

    const int16_t heightDiff = target - distance;
    if (abs(heightDiff) <= DESK_HEIGHT_TOLERANCE) {
        deskMoveEnd("OK");
        return;
    }

    const bool shouldGoUp = heightDiff > 0;
    if (shouldGoUp != goingUp) {
        deskMoveEnd("OVERSHOOT");
        return;
    }
}
