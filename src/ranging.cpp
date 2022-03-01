#include <Wire.h>
#include <Adafruit_VL53L1X.h>

#include "ranging.h"
#include "config.h"
#include "util.h"
#include "mqtt.h"

#define RANGING_UNUSED_TIMEOUT 2000

Adafruit_VL53L1X vl53 = Adafruit_VL53L1X();

static ranging_result_t lastValue;
static unsigned long lastQueryTime = 0;
static TaskHandle_t rangingTaskHandle;
static bool isRanging = false;

static void rangingStart()
{
    vl53.clearInterrupt();
    vl53.startRanging();
}

static void rangingStop()
{
    vl53.stopRanging();
    vl53.clearInterrupt();
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
            lastValue.valid = true;
            lastValue.value = vl53.distance();
            lastValue.time = millis();
            vl53.clearInterrupt();
        }
        else
        {
            uint8_t rangeStatus = 0;
            vl53.VL53L1X_GetRangeStatus(&rangeStatus);
            if (rangeStatus)
            {
                mqttSetLastError("VL53L1X_GetRangeStatus: " + String(rangeStatus));

                rangingStop();
                vl53.end();
                rangingSensorInit();
                rangingStart();
            }
        }

        delay(10);
    }

    rangingStop();
}

static void rangingTask(void *parameter)
{
    while (1)
    {
        if (shouldRange())
        {
            isRanging = true;
            rangingTaskInner();
        }
        else
        {
            isRanging = false;
        }
        delay(10);
    }
}

void rangingSetup()
{
    lastValue.valid = false;
    Wire.setPins(PIN_SDA, PIN_SCL);
    Wire.begin();
    rangingSensorInit();

    CREATE_TASK(rangingTask, "ranging", 5, &rangingTaskHandle);
}

static void rangingChecks()
{
    lastQueryTime = millis();
    isRanging = true;

    if (lastValue.value >= 0 && millis() - lastValue.time > RANGING_TIMEOUT)
    {
        lastValue.valid = false;
    }
}

const ranging_result_t rangingGetResult()
{
    rangingChecks();

    return lastValue;
}

const ranging_result_t rangingWaitForResult()
{
    rangingChecks();

    while (isRanging && !lastValue.valid)
    {
        delay(1);
    }

    return lastValue;
}
