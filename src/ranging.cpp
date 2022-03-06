#include <Wire.h>
#include <Adafruit_VL53L1X.h>

#include "ranging.h"
#include "config.h"
#include "util.h"
#include "mqtt.h"

#define RANGING_UNUSED_TIMEOUT 2000

Adafruit_VL53L1X vl53 = Adafruit_VL53L1X();

static ranging_result_t lastValue;
static ranging_result_t INVALID_VALUE;
static unsigned long lastQueryTime = 0;
static TaskHandle_t rangingTaskHandle;
static uint8_t rangeErrorRepeats = 0;
static uint8_t rangeLastError = 0;

static void rangingStart()
{
    vl53.startRanging();
}

static void rangingStop()
{
    vl53.stopRanging();
}

static void rangingSensorInit()
{
    if (!vl53.begin(0x29, &Wire))
    {
        mqttSetLastError("VL53L1X_Begin: " + String(vl53.vl_status));
        return;
    }

    rangingStop();

#ifdef RANGING_DISTANCE_MODE
    vl53.VL53L1X_SetDistanceMode(RANGING_DISTANCE_MODE);
#endif

#ifdef RANGING_TIMING_BUDGET
    vl53.setTimingBudget(RANGING_TIMING_BUDGET);
#endif

#ifdef RANGING_ROI_CENTER
    vl53.VL53L1X_SetROICenter(RANGING_ROI_CENTER);
#endif
#ifdef RANGING_ROI_WIDTH
    vl53.VL53L1X_SetROI(RANGING_ROI_WIDTH, RANGING_ROI_HEIGHT);
#endif
}

static bool shouldRange()
{
    return millis() - lastQueryTime <= RANGING_UNUSED_TIMEOUT;
}

static void rangingTaskInner()
{
    rangingStart();

    while (shouldRange())
    {
        if (vl53.dataReady())
        {
            lastValue.valid = false;
            lastValue.value = vl53.distance();
            lastValue.time = millis();
            lastValue.valid = true;
            vl53.clearInterrupt();
        }
        else
        {
            uint8_t rangeStatus = 0;
            vl53.VL53L1X_GetRangeStatus(&rangeStatus);
            if (rangeStatus)
            {
                mqttSetLastError("VL53L1X_GetRangeStatus: " + String(rangeStatus, HEX) + " [" + String(rangeErrorRepeats) + "]");
                if (rangeLastError == rangeStatus)
                {
                    rangeErrorRepeats++;
                    if (rangeErrorRepeats > RANGING_MAX_ERROR_REPEATS)
                    {
                        if (rangeStatus != 2 && rangeStatus != 4)
                        {
                            rangingStop();
                            vl53.end();
                            delay(100);
                            rangingSensorInit();
                            rangingStart();
                        }
                        rangeErrorRepeats = 0;
                    }
                }
                else
                {
                    rangeErrorRepeats = 0;
                    rangeLastError = rangeStatus;
                }
            }
            else
            {
                rangeErrorRepeats = 0;
                rangeLastError = 0;
            }
        }

        delay(10);
    }

    rangingStop();
}

static void rangingTask(void *parameter)
{
    Wire.begin(PIN_SDA, PIN_SCL, 10000U);
    rangingSensorInit();

    while (1)
    {
        rangeLastError = 0;
        rangeErrorRepeats = 0;
        if (shouldRange())
        {
            rangingTaskInner();
            lastValue.valid = false;
        }
        delay(10);
    }
}

void rangingSetup()
{
    lastValue.valid = false;
    INVALID_VALUE.valid = false;
    CREATE_TASK(rangingTask, "ranging", 50, &rangingTaskHandle);
}

static void rangingChecks()
{
    lastQueryTime = millis();

    if (lastValue.valid && millis() - lastValue.time > RANGING_TIMEOUT)
    {
        lastValue.valid = false;
    }
}

const ranging_result_t rangingWaitForNewResult(const unsigned long lastTime, unsigned long timeout)
{
    const unsigned long startTime = millis();
    rangingChecks();

    if (timeout == 0)
    {
        timeout = RANGING_TIMEOUT;
    }

    while (!lastValue.valid || lastValue.time == lastTime)
    {
        lastQueryTime = millis();
        delay(1);
        if (millis() - startTime > timeout)
        {
            return INVALID_VALUE;
        }
    }

    return lastValue;
}

const ranging_result_t rangingWaitForNextResult(unsigned long timeout)
{
    return rangingWaitForNewResult(lastValue.time, timeout);
}

const ranging_result_t rangingWaitForAnyResult(unsigned long timeout)
{
    const unsigned long startTime = millis();
    rangingChecks();

    if (timeout == 0)
    {
        timeout = RANGING_TIMEOUT;
    }

    while (!lastValue.valid)
    {
        lastQueryTime = millis();
        delay(1);
        if (millis() - startTime > timeout)
        {
            return INVALID_VALUE;
        }
    }

    return lastValue;
}
