#include <Wire.h>
#include <VL53L1X_ULD.h>

#include "ranging.h"
#include "config.h"
#include "util.h"
#include "mqtt.h"

VL53L1X_ULD vl53 = VL53L1X_ULD();

static ranging_result_t lastValue;
static ranging_result_t INVALID_VALUE;
static unsigned long lastQueryTime = 0;
static TaskHandle_t rangingTaskHandle;
static uint8_t rangeErrorRepeats = 0;
static uint8_t rangeLastError = 0;

static void rangingStart()
{
    vl53.ClearInterrupt();
    vl53.StartRanging();
}

static void rangingStop()
{
    vl53.StopRanging();
}

static void rangingSensorInit()
{
    VL53L1_Error status = vl53.Begin(0x29);
    if (status != VL53L1_ERROR_NONE)
    {
        mqttSetLastError("VL53L1X_Begin: " + String(status));
        return;
    }

    rangingStop();

#ifdef RANGING_DISTANCE_MODE
    vl53.SetDistanceMode(RANGING_DISTANCE_MODE);
#endif

#ifdef RANGING_TIMING_BUDGET
    vl53.SetTimingBudgetInMs(RANGING_TIMING_BUDGET);
    vl53.SetInterMeasurementInMs(RANGING_TIMING_BUDGET);
#endif

#ifdef RANGING_ROI_CENTER
    vl53.SetROICenter(RANGING_ROI_CENTER);
#endif
#ifdef RANGING_ROI_WIDTH
    vl53.SetROI(RANGING_ROI_WIDTH, RANGING_ROI_HEIGHT);
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
        uint8_t dataReady = false;
        vl53.CheckForDataReady(&dataReady);
        if (dataReady)
        {
            lastValue.valid = false;
            vl53.GetDistanceInMm(&lastValue.value);
            lastValue.time = millis();
            lastValue.valid = true;
            vl53.ClearInterrupt();
        }
        else
        {
            ERangeStatus rangeStatus;
            vl53.GetRangeStatus(&rangeStatus);
            if (rangeStatus != RangeValid)
            {
                mqttSetLastError("VL53L1X_GetRangeStatus: " + String(rangeStatus, HEX) + " [" + String(rangeErrorRepeats) + "]");
                if (rangeLastError == rangeStatus)
                {
                    rangeErrorRepeats++;
                    if (rangeErrorRepeats > RANGING_MAX_ERROR_REPEATS && rangeStatus >= 8)
                    {
                        rangeErrorRepeats = 0;
                        rangingStop();
                        delay(200);
                        rangingSensorInit();
                        rangingStart();
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
    Wire.begin(PIN_SDA, PIN_SCL);
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
