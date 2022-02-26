#include <Arduino.h>

#include "util.h"
#include "desk.h"

bool doRestart(bool force)
{
    Serial.println("Restarting...");
    if (deskGetMovingDirection() && !force)
    {
        return false;
    }
    deskStop();
    ESP.restart();
    return true;
}
