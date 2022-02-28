#include <Arduino.h>

#include "buttons.h"
#include "config.h"

void buttonsSetup()
{
    pinMode(PIN_ARROW_UP, INPUT_PULLDOWN);
    pinMode(PIN_ARROW_DOWN, INPUT_PULLDOWN);
    pinMode(PIN_BUTTON, INPUT_PULLDOWN);
}
