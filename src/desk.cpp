#include <Arduino.h>
#include <ArduinoJson.h>

#include "desk.h"
#include "config.h"
#include "ranging.h"
#include "mqtt.h"
#include "util.h"

static int16_t startDistance;
static unsigned long timeout;
static unsigned long startTime;
static int failedSpeedTries;
static int16_t speedLastDistance;
static unsigned long speedLastTime;
static unsigned long rangingLastTime;
static int16_t target;
static int8_t deskMoving;
static char mqttId[128];
static TaskHandle_t moveTaskHandle;

static void deskStopInternal()
{
    deskMoving = 0;
    digitalWrite(PIN_RELAY_UP, LOW);
    digitalWrite(PIN_RELAY_DOWN, LOW);
}

void deskStop()
{
    deskStopInternal();
    if (moveTaskHandle)
    {
        vTaskDelete(moveTaskHandle);
        moveTaskHandle = NULL;
        mqttSendJSON(mqttId, "adjust:stop", "STOPPED");
    }
}

void deskSetup()
{
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    deskStop();
}

void deskMoveTask(void *parameter)
{
    String stopReason = "UNKNOWN";

    while (1)
    {
        delay(10);

        if (!deskMoving)
        {
            stopReason = "STOPPED";
            break;
        }

        const unsigned long time = millis();
        if (time - startTime > timeout)
        {
            stopReason = "MAIN TIMEOUT";
            break;
        }

        const int16_t distance = rangingGetDistance();
        if (distance < 0)
        {
            if (time - rangingLastTime > DESK_RANGING_TIMEOUT)
            {
                stopReason = "RANGING TIMEOUT";
                break;
            }
            continue;
        }
        rangingLastTime = time;

        const int16_t heightDiff = abs(target - distance);
        const int8_t shouldMoveDirection = (target > distance) ? 1 : -1;
        const bool inFineAdjust = heightDiff <= DESK_FINE_ADJUST_RANGE;

        if (time - speedLastTime >= DESK_CALCULATE_SPEED_TIME)
        {
            const double speed = abs((double)(distance - speedLastDistance) / (double)(time - speedLastTime));
            speedLastDistance = distance;
            speedLastTime = time;

            if (speed < DESK_SPEED_MIN)
            {
                failedSpeedTries++;
                if (failedSpeedTries >= DESK_SPEED_TRIES)
                {
                    stopReason = "SPEED TO LOW";
                    break;
                }
            }
            else
            {
                failedSpeedTries = 0;
            }

            if (!inFineAdjust)
            {
                mqttSendJSON(mqttId, "adjust:move", String(speed).c_str(), distance);
            }
        }

        if (heightDiff <= DESK_HEIGHT_TOLERANCE)
        {
            stopReason = "DONE";
            break;
        }

        if (shouldMoveDirection != deskMoving)
        {
            stopReason = "OVERSHOOT";
            break;
        }
    }

    deskStopInternal();
    moveTaskHandle = NULL;

    mqttSendJSON(mqttId, "adjust:stop", stopReason.c_str());

    mqttId[0] = 0;

    vTaskDelete(NULL);
}

void deskAdjustHeight(int16_t _target, const char *_mqttId)
{
    deskStop();

    if (_mqttId)
    {
        strcpy(mqttId, _mqttId);
    }
    else
    {
        mqttId[0] = 0;
    }

    if (_target < DESK_HEIGHT_MIN || _target > DESK_HEIGHT_MAX)
    {
        return;
    }

    target = _target;
    startTime = millis();

    startDistance = rangingWaitAndGetDistance();

    timeout = abs(target - startDistance) * DESK_ADJUST_TIMEOUT_PER_MM;

    deskMoving = (target > startDistance) ? 1 : -1;

    mqttSendJSON(mqttId, "adjust:start", "");

    if (abs(target - startDistance) < DESK_HEIGHT_TOLERANCE)
    {
        mqttSendJSON(mqttId, "adjust:stop", "NO CHANGE");
        return;
    }

    if (deskMoving > 0)
    {
        digitalWrite(PIN_RELAY_DOWN, LOW);
        digitalWrite(PIN_RELAY_UP, HIGH);
    }
    else
    {
        digitalWrite(PIN_RELAY_UP, LOW);
        digitalWrite(PIN_RELAY_DOWN, HIGH);
    }

    failedSpeedTries = 0;
    speedLastDistance = startDistance;
    speedLastTime = startTime;
    rangingLastTime = startTime;

    CREATE_TASK(deskMoveTask, "deskMove", 10, &moveTaskHandle);
}

int8_t deskIsMoving()
{
    return deskMoving;
}

int16_t deskGetTarget()
{
    return target;
}
