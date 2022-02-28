#include <Wire.h>
#include <Adafruit_VL53L1X.h>

#include "ranging.h"
#include "config.h"
#include "util.h"
#include "mqtt.h"

#define RANGING_UNUSED_TIMEOUT 2000

Adafruit_VL53L1X vl53 = Adafruit_VL53L1X();

static int16_t lastValue = -1;
static unsigned long lastValueTime = 0;
static unsigned long lastQueryTime = 0;
static TaskHandle_t rangingTaskHandle;

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

void rangingSetup()
{
    Wire.setPins(PIN_SDA, PIN_SCL);
    Wire.begin();
    rangingSensorInit();
}

void rangingTask(void *parameter)
{
    rangingStart();

    while (1)
    {
        delay(10);

        if (millis() - lastQueryTime > RANGING_UNUSED_TIMEOUT)
        {
            break;
        }

        if (vl53.dataReady())
        {
            lastValue = vl53.distance();
            lastValueTime = millis();
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
    }

    rangingStop();
    rangingTaskHandle = NULL;
    vTaskDelete(NULL);
}

static void rangingChecks()
{
    lastQueryTime = millis();

    if (lastValue >= 0 && millis() - lastValueTime > RANGING_TIMEOUT)
    {
        lastValue = -1;
        lastValueTime = -1;
    }

    if (!rangingTaskHandle)
    {
        CREATE_TASK(rangingTask, "ranging", 5, &rangingTaskHandle);
    }
}

int16_t rangingGetDistance()
{
    rangingChecks();

    return lastValue;
}

int16_t rangingWaitAndGetDistance()
{
    rangingChecks();

    while (lastValue < 0)
    {
        delay(1);
    }

    return lastValue;
}
