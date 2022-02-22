#include <Wire.h>
#include <Adafruit_VL53L1X.h>

#include "ranging.h"
#include "config.h"

Adafruit_VL53L1X vl53 = Adafruit_VL53L1X();

static int16_t lastValue = -1;
static unsigned long lastValueTime = -1;

void rangingSetup() {
    Wire.setPins(PIN_SDA, PIN_SCL);
    Wire.begin();

    if (!vl53.begin(0x29, &Wire)) {
        Serial.println("VL53L1X init failed: ");
        Serial.println(vl53.vl_status);
    }

    Serial.print("Sensor ID: 0x");
    Serial.println(vl53.sensorID(), HEX);

    vl53.stopRanging();
    vl53.clearInterrupt();

#ifdef RANGING_DISTANCE_MODE
    vl53.VL53L1X_SetDistanceMode(RANGING_DISTANCE_MODE);
#endif

#ifdef RANGING_TIMING_BUDGET
    vl53.setTimingBudget(RANGING_TIMING_BUDGET);
#endif

#ifdef RANGING_ROI_CENTER
    vl53.VL53L1X_SetROICenter(RANGING_ROI_CENTER);
    vl53.VL53L1X_SetROI(RANGING_ROI_WIDTH, RANGING_ROI_HEIGHT);
#endif
}

void rangingStart() {
    lastValue = -1;
    lastValueTime = -1;
    vl53.startRanging();
}

void rangingStop() {
    vl53.stopRanging();
    lastValue = -1;
    lastValueTime = -1;
}

void rangingLoop() {
    if (vl53.dataReady()) {
        lastValue = vl53.distance();
        lastValueTime = millis();
        vl53.clearInterrupt();
    }
}

static void rangingCheckTimeout() {
    if (lastValue >= 0 && millis() - lastValueTime > RANGING_TIMEOUT) {
        lastValue = -1;
        lastValueTime = -1;
    }
}

int16_t rangingGetDistance() {
    rangingCheckTimeout();

    return lastValue;
}

int16_t rangingWaitAndGetDistance() {
    rangingCheckTimeout();

    while (lastValue < 0) {
        rangingLoop();
        delay(1);
    }
    return lastValue;
}
