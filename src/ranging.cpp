#include <Wire.h>
#include <VL53L1X_ULD.h>

#include "ranging.h"
#include "config.h"
#include "util.h"
#include "mqtt.h"

VL53L1X_ULD vl53 = VL53L1X_ULD();

static ranging_result_t lastValue;
static ranging_result_t INVALID_VALUE;
static TaskHandle_t rangingTaskHandle;
static uint8_t rangingRequirementsBit = 0;

void rangingAcquireBit(uint8_t bit)
{
    rangingRequirementsBit |= bit;
}

void rangingReleaseBit(uint8_t bit)
{
    rangingRequirementsBit &= ~bit;
}

static void rangingSensorInit()
{
    VL53L1_Error status = vl53.Begin(0x29);
    if (status != VL53L1_ERROR_NONE)
    {
        mqttSetLastError("VL53L1X_Begin: " + String(status));
        return;
    }
    SERIAL_PORT.println("VL53L1X initialized");

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

#ifdef RANGING_OFFSET
    vl53.SetOffsetInMm(RANGING_OFFSET);
#endif

    SERIAL_PORT.println("VL53L1X configured");
}

static void rangingTaskInner()
{
    vl53.ClearInterrupt();
    vl53.StartRanging();
    mqttSetDebug("RANGING START");

    while (rangingRequirementsBit)
    {
        uint8_t dataReady = false;
        VL53L1_Error status = vl53.CheckForDataReady(&dataReady);
        if (status == VL53L1_ERROR_NONE && dataReady)
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
            if (rangeStatus != RangeValid || status != VL53L1_ERROR_NONE) {
                mqttSetLastError("VL53L1X_GetRangeStatus: " + String(rangeStatus, HEX) + " <" + String(status, HEX) + ">");
            }
        }

        delay(10);
    }

    mqttSetDebug("RANGING STOP");
    vl53.StopRanging();
}

static void rangingTask(void *parameter)
{
    Wire.begin(PIN_SDA, PIN_SCL);
    rangingSensorInit();

    while (1)
    {
        if (rangingRequirementsBit)
        {
            rangingTaskInner();
            lastValue.valid = false;
            delay(RANGING_TIMING_BUDGET);
        }
        delay(10);
    }
}

static void rangingInitTask(void *parameter)
{
    rangingAcquireBit(RANGING_BIT_INIT);
    rangingWaitForNextResult(1000);
    rangingWaitForNextResult(1000);
    rangingWaitForNextResult(1000);
    rangingWaitForNextResult(1000);
    rangingWaitForNextResult(1000);
    rangingReleaseBit(RANGING_BIT_INIT);

    mqttDoHeightUpdate();

    SERIAL_PORT.println("Ranging init complete!");

    vTaskDelete(NULL);
}

void rangingSetup()
{
    lastValue.valid = false;
    INVALID_VALUE.valid = false;
    CREATE_TASK(rangingTask, "ranging", 50, &rangingTaskHandle);
    CREATE_TASK(rangingInitTask, "rangingInit", 10, NULL);
}

static bool rangingChecks()
{
    if (!rangingRequirementsBit)
    {
        return false;
    }

    if (lastValue.valid && millis() - lastValue.time > RANGING_TIMEOUT)
    {
        lastValue.valid = false;
    }

    return true;
}

const ranging_result_t rangingWaitForNewResult(const unsigned long lastTime, unsigned long timeout)
{
    const unsigned long startTime = millis();
    if (!rangingChecks())
    {
        return INVALID_VALUE;
    }

    if (timeout == 0)
    {
        timeout = RANGING_TIMEOUT;
    }

    while (!lastValue.valid || lastValue.time == lastTime)
    {
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
    if (!rangingChecks())
    {
        return INVALID_VALUE;
    }

    if (timeout == 0)
    {
        timeout = RANGING_TIMEOUT;
    }

    while (!lastValue.valid)
    {
        delay(1);
        if (millis() - startTime > timeout)
        {
            return INVALID_VALUE;
        }
    }

    return lastValue;
}
