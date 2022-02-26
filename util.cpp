#include <Arduino.h>

#include "util.h"
#include "desk.h"

bool safeRestart()
{
    Serial.println("Restarting...");
    if (deskGetMovingDirection())
    {
        return false;
    }
    deskStop();
    ESP.restart();
    return true;
}
