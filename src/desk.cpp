#include <Arduino.h>
#include <ArduinoJson.h>

#include "desk.h"
#include "config.h"
#include "ranging.h"
#include "mqtt.h"

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
static char mqttId[128];

static void deskStopInternal() {
    deskMoving = false;
    digitalWrite(PIN_RELAY_UP, LOW);
    digitalWrite(PIN_RELAY_DOWN, LOW);
}

void deskSetup() {
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    deskStopInternal();
}

void deskAdjustHeight(int16_t _target, const char *_mqttId) {
    deskStop();

    if (_mqttId) {
        strcpy(mqttId, _mqttId);
    } else {
        mqttId[0] = 0;
    }

    if (_target < DESK_HEIGHT_MIN || _target > DESK_HEIGHT_MAX) {
        return;
    }

    target = _target;
    startTime = millis();

    rangingStart();
    startDistance = rangingWaitAndGetDistance();

    timeout = abs(target - startDistance) * DESK_ADJUST_TIMEOUT_PER_MM;

    mqttSendJSON(mqttId, "adjust:start", "");

    if (abs(target - startDistance) < DESK_HEIGHT_TOLERANCE) {
        deskStopInternal();
        rangingStop();
        mqttSendJSON(mqttId, "adjust:stop", "NO CHANGE");
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
    deskStopInternal();

    mqttSendJSON(mqttId, "adjust:stop", reason.c_str());

    mqttId[0] = 0;
    rangingStop();
}

void deskStop() {
    if (!deskMoving) {
        return;
    }
    deskMoveEnd("STOPPED");
}

static inline bool deskLoopInternal() {
    if (!deskMoving) {
        return false;
    }

    const unsigned long time = millis();
    if (time - startTime > timeout) {
        deskMoveEnd("MAIN TIMEOUT");
        return false;
    }

    const int16_t distance = rangingGetDistance();
    if (distance < 0) {
        if (time - rangingLastTime > DESK_RANGING_TIMEOUT) {
            deskMoveEnd("RANGING TIMEOUT");
        }
        return false;
    }
    rangingLastTime = time;

    const int16_t heightDiff = abs(target - distance);
    const bool shouldGoUp = target > distance;
    const bool inFineAdjust = heightDiff <= DESK_FINE_ADJUST_RANGE;

    if (time - speedLastTime >= DESK_CALCULATE_SPEED_TIME) {
        const double speed = abs((double)(distance - speedLastDistance) / (double)(time - speedLastTime));
        speedLastDistance = distance;
        speedLastTime = time;

        if (speed < DESK_SPEED_MIN) {
            failedSpeedTries++;
            if (failedSpeedTries >= DESK_SPEED_TRIES) {
                deskMoveEnd("SPEED TO LOW");
                return false;
            }
        } else {
            failedSpeedTries = 0;
        }

        if (!inFineAdjust) {
            mqttSendJSON(mqttId, "adjust:move", String(speed).c_str(), distance);
        }
    }

    if (heightDiff <= DESK_HEIGHT_TOLERANCE) {
        deskMoveEnd("OK");
        return false;
    }

    if (shouldGoUp != goingUp) {
        deskMoveEnd("OVERSHOOT");
        return false;
    }

    return inFineAdjust;
}

void deskLoop() {
    while (deskLoopInternal()) {
        delay(1);
        rangingLoop();
    }
}

bool deskIsMoving() {
    return deskMoving;
}
