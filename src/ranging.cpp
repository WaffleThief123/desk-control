#include <Wire.h>
#include <Adafruit_VL53L1X.h>

#include "ranging.h"
#include "config.h"

Adafruit_VL53L1X vl53 = Adafruit_VL53L1X();

void rangingSetup() {
    Wire.setPins(PIN_SDA, PIN_SCL);
    Wire.begin();

    if (!vl53.begin(0x29, &Wire)) {
        Serial.println("VL53L1X init failed: ");
        Serial.println(vl53.vl_status);
    }

    Serial.print("Sensor ID: 0x");
    Serial.println(vl53.sensorID(), HEX);

    vl53.setTimingBudget(50);
}

void rangingStart() {
    vl53.startRanging();
}

void rangingStop() {
    vl53.stopRanging();
}

int16_t rangingGetDistance() {
    while (!vl53.dataReady()) {
        delay(1);
    }
    int16_t res = vl53.distance();
    vl53.clearInterrupt();
    return res;
}
