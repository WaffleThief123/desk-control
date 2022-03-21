#include <Arduino.h>

#include "config.h"
#include "util.h"
#include "desk.h"

bool debugEnabled = false;

bool doRestart()
{
    SERIAL_PORT.println("Restarting...");
    ESP.restart();
    return true;
}
