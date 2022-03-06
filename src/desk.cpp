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
static int16_t target;
static int8_t deskMovingDirection;
static char mqttId[128];
static TaskHandle_t moveTaskHandle;
static TaskHandle_t moveStatusTaskHandle;
static double deskSpeed;
static unsigned long rangingLastTime;

static void deskStopInternal()
{
    deskMovingDirection = 0;
    digitalWrite(PIN_RELAY_UP, LOW);
    digitalWrite(PIN_RELAY_DOWN, LOW);
}

void deskStop()
{
    deskStopInternal();
    if (moveStatusTaskHandle != NULL)
    {
        vTaskDelete(moveStatusTaskHandle);
        moveStatusTaskHandle = NULL;
        rangingReleaseBit(RANGING_BIT_DESK_STATUS);
    }
    if (moveTaskHandle != NULL)
    {
        vTaskDelete(moveTaskHandle);
        moveTaskHandle = NULL;
        mqttSendJSON(mqttId, "adjust:stop", "STOPPED");
        mqttId[0] = 0;
        rangingReleaseBit(RANGING_BIT_DESK_MOVE);
    }
}

static void deskMoveTask(void *parameter)
{
    rangingAcquireBit(RANGING_BIT_DESK_MOVE);
    String stopReason = "STOPPED";

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

    while (deskMovingDirection)
    {
        if (millis() - startTime > timeout)
        {
            stopReason = "MAIN TIMEOUT";
            break;
        }

        const ranging_result_t rangingResult = rangingWaitForNewResult(rangingLastTime, DESK_RANGING_TIMEOUT);
        if (!rangingResult.valid)
        {
            stopReason = "RANGING TIMEOUT";
            break;
        }
        const int16_t distance = rangingResult.value;
        rangingLastTime = rangingResult.time;

        const unsigned long time = rangingResult.time;

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

        delay(1);
    }

    deskStopInternal();

    mqttSendJSON(mqttId, "adjust:stop", stopReason.c_str());

    mqttId[0] = 0;

    moveTaskHandle = NULL;
    rangingReleaseBit(RANGING_BIT_DESK_MOVE);
    vTaskDelete(NULL);
}

static void deskMoveStatusTask(void *parameter)
{
    rangingAcquireBit(RANGING_BIT_DESK_STATUS);
    delay(100);
    unsigned long lastRangeResultTime = 0;
    while (deskMovingDirection)
    {
        const ranging_result_t rangingResult = rangingWaitForNewResult(lastRangeResultTime);
        if (rangingResult.valid)
        {
            lastRangeResultTime = rangingResult.time;
            mqttSendJSON(mqttId, "adjust:move", String(deskSpeed).c_str(), rangingResult.value);
        }
        delay(100);
    }

    moveStatusTaskHandle = NULL;
    rangingReleaseBit(RANGING_BIT_DESK_STATUS);
    vTaskDelete(NULL);
}

void deskSetup()
{
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    deskStop();
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

    rangingAcquireBit(RANGING_BIT_DESK_MOVE);
    target = _target;

    const ranging_result_t rangingResult = rangingWaitForNextResult();
    if (!rangingResult.valid)
    {
        mqttSendJSON(mqttId, "adjust:stop", "INITIAL RANGING TIMEOUT", -1);
        return;
    }

    startTime = rangingResult.time;
    startDistance = rangingResult.value;

    timeout = abs(target - startDistance) * DESK_ADJUST_TIMEOUT_PER_MM;

    if (abs(target - startDistance) < DESK_HEIGHT_TOLERANCE)
    {
        mqttSendJSON(mqttId, "adjust:stop", "NO CHANGE", startDistance);
        rangingReleaseBit(RANGING_BIT_DESK_MOVE);
        return;
    }

    deskMovingDirection = (target > startDistance) ? 1 : -1;
    mqttSendJSON(mqttId, "adjust:start", "", startDistance);

    deskSpeed = 0;
    failedSpeedTries = 0;
    speedLastDistance = startDistance;
    speedLastTime = startTime;
    rangingLastTime = startTime;

    CREATE_TASK(deskMoveTask, "deskMove", 100, &moveTaskHandle);
    CREATE_TASK_IO(deskMoveStatusTask, "deskMoveStatus", 20, &moveStatusTaskHandle);
}

int8_t deskGetMovingDirection()
{
    return deskMovingDirection;
}

int16_t deskGetTarget()
{
    return target;
}
