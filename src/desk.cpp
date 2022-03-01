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
static int8_t deskMovingDirection;
static char mqttId[128];
static TaskHandle_t moveTaskHandle;
static TaskHandle_t moveStatusTaskHandle;
static double deskSpeed;

void deskStop()
{
    deskMovingDirection = 0;
    digitalWrite(PIN_RELAY_UP, LOW);
    digitalWrite(PIN_RELAY_DOWN, LOW);
}

static void deskMoveTaskInner()
{
    String stopReason = "STOPPED";

    while (deskMovingDirection)
    {
        if (millis() - startTime > timeout)
        {
            stopReason = "MAIN TIMEOUT";
            break;
        }

        const ranging_result_t rangingResult = rangingGetResult();
        const int16_t distance = rangingResult.value;
        if (!rangingResult.valid)
        {
            if (millis() - rangingLastTime > DESK_RANGING_TIMEOUT)
            {
                stopReason = "RANGING TIMEOUT";
                break;
            }
            continue;
        }

        const unsigned long time = rangingResult.time;

        rangingLastTime = time;

        const int16_t heightDiff = abs(target - distance);
        const int8_t shouldMoveDirection = (target > distance) ? 1 : -1;

        if (heightDiff <= DESK_HEIGHT_TOLERANCE)
        {
            stopReason = "DONE";
            break;
        }

        if (shouldMoveDirection != deskMovingDirection)
        {
            stopReason = "OVERSHOOT";
            break;
        }

        if (time - speedLastTime >= DESK_CALCULATE_SPEED_TIME)
        {
            const double speed = ((double)(distance - speedLastDistance) / (double)(time - speedLastTime)) * 1000.0;
            speedLastDistance = distance;
            speedLastTime = time;
            deskSpeed = speed;

            const int8_t speedDirection = (speed > 0) ? 1 : -1;

            if (abs(speed) < DESK_SPEED_MIN || speedDirection != deskMovingDirection)
            {
                failedSpeedTries++;
                if (failedSpeedTries >= DESK_SPEED_TRIES)
                {
                    if (speedDirection != deskMovingDirection && abs(speed) >= DESK_SPEED_MIN)
                    {
                        stopReason = "MOVING BACKWARDS";
                    }
                    else
                    {
                        stopReason = "SPEED TOO LOW";
                    }
                    break;
                }
            }
            else
            {
                failedSpeedTries = 0;
            }
        }

        delay(10);
    }

    deskStop();

    mqttSendJSON(mqttId, "adjust:stop", stopReason.c_str());

    mqttId[0] = 0;
}

static void deskMoveTask(void* parameter)
{
    while (1)
    {
        if (deskMovingDirection)
        {
            deskMoveTaskInner();
        }
        delay(10);
    }
}

static void deskMoveStatusTask(void* parameter)
{
    while (1)
    {
        if (deskMovingDirection)
        {
            mqttSendJSON(mqttId, "adjust:move", String(deskSpeed).c_str());
        }
        delay(100);
    }
}

void deskSetup()
{
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    deskStop();

    CREATE_TASK(deskMoveTask, "deskMove", 10, &moveTaskHandle);
    CREATE_TASK_IO(deskMoveStatusTask, "deskMoveStatus", 1, &moveStatusTaskHandle);
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

    const ranging_result_t rangingResult = rangingWaitForResult();
    if (!rangingResult.valid)
    {
        mqttSendJSON(mqttId, "adjust:stop", "INITIAL RANGING TIMEOUT");
        return;
    }

    startDistance = rangingResult.value;

    timeout = abs(target - startDistance) * DESK_ADJUST_TIMEOUT_PER_MM;

    if (abs(target - startDistance) < DESK_HEIGHT_TOLERANCE)
    {
        mqttSendJSON(mqttId, "adjust:stop", "NO CHANGE", startDistance);
        return;
    }

    deskMovingDirection = (target > startDistance) ? 1 : -1;
    mqttSendJSON(mqttId, "adjust:start", "", startDistance);

    if (deskMovingDirection > 0)
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
}

int8_t deskGetMovingDirection()
{
    return deskMovingDirection;
}

int16_t deskGetTarget()
{
    return target;
}
