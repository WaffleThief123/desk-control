#include <Arduino.h>

#include "config.h"
#include "util.h"
#include "desk.h"

bool debugEnabled = false;

bool doRestart(bool force)
{
    SERIAL_PORT.println("Restarting...");
    if (deskGetMovingDirection() && !force)
    {
        SERIAL_PORT.println("Can't restart, desk moving and not forced!");
        return false;
    }
    deskStop();
    ESP.restart();
    return true;
}
