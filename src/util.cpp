#include <Arduino.h>

#include "util.h"
#include "desk.h"

bool doRestart(bool force)
{
    Serial.println("Restarting...");
    if (deskGetMovingDirection() && !force)
    {
        Serial.println("Can't restart, desk moving and not forced!");
        return false;
    }
    deskStop();
    ESP.restart();
    return true;
}
