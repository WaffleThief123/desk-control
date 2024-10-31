#include <Arduino.h>
#include <ArduinoJson.h>

#include "desk.h"
#include "config.h"
#include "ranging.h"
#include "mqtt.h"
#include "util.h"

static unsigned long startTime;
static int failedSpeedTries;
static int16_t speedLastDistance;
static unsigned long speedLastTime;
static int16_t target;
static int8_t deskMovingDirection;
static TaskHandle_t moveTaskHandle;
static TaskHandle_t moveStatusTaskHandle;
static double deskSpeed;
static unsigned long rangingLastTime;
static SemaphoreHandle_t deskAdjustMutex;

// #define DESK_UP_LEDC 0
// #define DESK_DOWN_LEDC 1
#define DESK_LEDC_FREQ 30000
#define DESK_LEDC_RES 8
#define DESK_LEDC_MIN ((1 << DESK_LEDC_RES) - 1)
#define DESK_LEDC_MAX 0

#define DESK_DECREASE_PER_MM ((DESK_SPEED_FULL - DESK_SPEED_SLOW) / (DESK_SLOWDOWN_START - DESK_SLOWDOWN_END))

static void deskStopInternal()
{
    deskMovingDirection = 0;
    ledcWrite(PIN_RELAY_UP, DESK_LEDC_MIN);
    ledcWrite(PIN_RELAY_DOWN, DESK_LEDC_MIN);
    mqttSetDebug("DESK OFF");
    mqttDoHeightUpdate();
}

static void deskStopWait()
{
    deskMovingDirection = 0;

    while (moveTaskHandle != NULL || moveStatusTaskHandle != NULL)
    {
        delay(1);
    }

    deskStopInternal();
}

void deskStop()
{
    if (!xSemaphoreTake(deskAdjustMutex, portMAX_DELAY))
    {
        return;
    }
    deskStopWait();
    xSemaphoreGive(deskAdjustMutex);
}

static void deskMoveTask(void *parameter)
{
    rangingAcquireBit(RANGING_BIT_DESK_MOVE);
    String stopReason = "STOPPED";

    ledcWrite(PIN_RELAY_DOWN, DESK_LEDC_MIN);
    ledcWrite(PIN_RELAY_UP, DESK_LEDC_MIN);

    uint32_t setDeskSpeed = UINT32_MAX;

    while (deskMovingDirection)
    {
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

        uint32_t deskSpeed = DESK_SPEED_FULL;
        if (heightDiff <= DESK_SLOWDOWN_END)
        {
            deskSpeed = DESK_SPEED_SLOW;
        }
        else if (heightDiff <= DESK_SLOWDOWN_START)
        {
            deskSpeed = DESK_SPEED_SLOW + (DESK_DECREASE_PER_MM * (heightDiff - DESK_SLOWDOWN_END));
        }

        deskSpeed = DESK_LEDC_MIN - deskSpeed;

        if (setDeskSpeed != deskSpeed)
        {
            if (deskMovingDirection > 0)
            {
                ledcWrite(PIN_RELAY_UP, deskSpeed);
            }
            else
            {
                ledcWrite(PIN_RELAY_DOWN, deskSpeed);
            }
            setDeskSpeed = deskSpeed;
        }

        delay(1);
    }

    deskStopInternal();

    mqttDoHeightUpdate();
    mqttSetStopReason(stopReason);

    delay(100);

    rangingReleaseBit(RANGING_BIT_DESK_MOVE);
    moveTaskHandle = NULL;
    vTaskDelete(NULL);
}

static void deskMoveStatusTask(void *parameter)
{
    rangingAcquireBit(RANGING_BIT_DESK_STATUS);
    uint8_t ctr = 0;
    while (deskMovingDirection)
    {
        if (++ctr >= 10)
        {
            ctr = 0;
            mqttDoHeightUpdate();
        }
        delay(10);
    }

    rangingReleaseBit(RANGING_BIT_DESK_STATUS);
    moveStatusTaskHandle = NULL;
    vTaskDelete(NULL);
}

void deskSetup()
{
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    digitalWrite(PIN_RELAY_UP, HIGH);
    digitalWrite(PIN_RELAY_DOWN, HIGH);

    ledcAttach(PIN_RELAY_UP, DESK_LEDC_FREQ, DESK_LEDC_RES);
    ledcAttach(PIN_RELAY_DOWN, DESK_LEDC_FREQ, DESK_LEDC_RES);

    deskStopInternal();
    esp_register_shutdown_handler(deskStopInternal);
    deskAdjustMutex = xSemaphoreCreateMutex();
}

void deskAdjustHeight(int16_t _target)
{
    if (!xSemaphoreTake(deskAdjustMutex, portMAX_DELAY))
    {
        return;
    }

    rangingAcquireBit(RANGING_BIT_DESK_ADJUST);

    deskStopWait();

    if (_target < DESK_HEIGHT_MIN || _target > DESK_HEIGHT_MAX)
    {
        mqttSetLastError("TARGET OUT OF BOUNDS");
        rangingReleaseBit(RANGING_BIT_DESK_ADJUST);
        xSemaphoreGive(deskAdjustMutex);
        return;
    }

    rangingAcquireBit(RANGING_BIT_DESK_MOVE);
    target = _target;

    const ranging_result_t rangingResult = rangingWaitForNextResult();
    if (!rangingResult.valid)
    {
        mqttSetLastError("INITIAL RANGING TIMEOUT");
        mqttDoHeightUpdate();
        rangingReleaseBit(RANGING_BIT_DESK_MOVE | RANGING_BIT_DESK_ADJUST);
        xSemaphoreGive(deskAdjustMutex);
        return;
    }

    startTime = rangingResult.time;
    const int16_t startDistance = rangingResult.value;

    if (abs(target - startDistance) < DESK_HEIGHT_TOLERANCE)
    {
        mqttDoHeightUpdate();
        rangingReleaseBit(RANGING_BIT_DESK_MOVE | RANGING_BIT_DESK_ADJUST);
        xSemaphoreGive(deskAdjustMutex);
        return;
    }

    deskMovingDirection = (target > startDistance) ? 1 : -1;
    mqttDoHeightUpdate();

    deskSpeed = 0;
    failedSpeedTries = 0;
    speedLastDistance = startDistance;
    speedLastTime = startTime;
    rangingLastTime = startTime;

    CREATE_TASK(deskMoveTask, "deskMove", 100, &moveTaskHandle);
    CREATE_TASK_IO(deskMoveStatusTask, "deskMoveStatus", 20, &moveStatusTaskHandle);
    rangingReleaseBit(RANGING_BIT_DESK_ADJUST);
    xSemaphoreGive(deskAdjustMutex);
}

int8_t deskGetMovingDirection()
{
    return deskMovingDirection;
}

int16_t deskGetTarget()
{
    return target;
}
