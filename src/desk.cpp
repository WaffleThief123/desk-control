#include <Arduino.h>

#include "desk.h"
#include "config.h"
#include "ranging.h"

static void deskStop() {
    digitalWrite(PIN_RELAY_UP, LOW);
    digitalWrite(PIN_RELAY_DOWN, LOW);
}

void deskSetup() {
    pinMode(PIN_RELAY_UP, OUTPUT);
    pinMode(PIN_RELAY_DOWN, OUTPUT);
    deskStop();
}

void deskAdjustHeight(int16_t target) {
    const unsigned long startTime = millis();

    rangingStart();
    const int16_t startDistance = rangingWaitAndGetDistance();

    const unsigned long timeout = abs(target - startDistance) * DESK_ADJUST_TIMEOUT_PER_MM;

    Serial.print("Adjusting desk height to ");
    Serial.print(target);
    Serial.print(" from ");
    Serial.print(startDistance);
    Serial.print(" with timeout ");
    Serial.println(timeout);

    if (abs(target - startDistance) < DESK_HEIGHT_TOLERANCE) {
        deskStop();
        rangingStop();
        Serial.println("Desk already at desired height!");
        return;
    }

    const bool goingUp = target > startDistance;
    if (goingUp) {
        digitalWrite(PIN_RELAY_DOWN, LOW);
        digitalWrite(PIN_RELAY_UP, HIGH);
    } else {
        digitalWrite(PIN_RELAY_UP, LOW);
        digitalWrite(PIN_RELAY_DOWN, HIGH);
    }

    int failedSpeedTries = 0;

    int16_t lastDistance = startDistance;
    unsigned long lastTime = millis();
    while (1) {
        const unsigned long time = millis();
        if (time - startTime > timeout) {
            Serial.println("TIMEOUT!");
            break;
        }

        const int16_t distance = rangingGetDistance();
        if (distance < 0) {
            continue;
        }
        
        if (time - lastTime >= DESK_CALCULATE_SPEED_TIME) {
            const double speed = abs((double)(distance - lastDistance) / (double)(time - lastTime));
            lastDistance = distance;
            lastTime = time;

            if (speed < DESK_SPEED_MIN) {
                failedSpeedTries++;
                if (failedSpeedTries >= DESK_SPEED_TRIES) {
                    Serial.println("UNDERSPEED!");
                    break;
                }
            } else {
                failedSpeedTries = 0;
            }
        }

        const int16_t heightDiff = target - distance;
        const bool shouldGoUp = heightDiff > 0;
    
        if (abs(heightDiff) <= DESK_HEIGHT_TOLERANCE || goingUp != shouldGoUp) {
            break;
        }

        delay(RANGING_TIMING_BUDGET);
    }
    deskStop();

    Serial.print("Desk height adjusted to ");
    Serial.print(rangingWaitAndGetDistance());
    Serial.print(" within ");
    Serial.println(millis() - startTime);

    rangingStop();
}
