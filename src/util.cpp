#include <Arduino.h>

#include "config.h"
#include "util.h"
#include "desk.h"

bool debugEnabled = true;

bool doRestart()
{
    SERIAL_PORT.println("Restarting...");
    ESP.restart();
    return true;
}
