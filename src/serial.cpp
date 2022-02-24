#include <Arduino.h>

#include "ranging.h"
#include "desk.h"
#include "mqtt.h"
#include "config.h"
#include "util.h"

String serialBuffer;

static void serialHandleCommand()
{
    serialBuffer.trim();
    serialBuffer.toLowerCase();

    Serial.print(serialBuffer);
    Serial.print(" ");

    if (serialBuffer.equals("range"))
    {
        mqttSendJSON(NULL, "range", "OK");
    }
    else if (serialBuffer.startsWith("adjust "))
    {
        deskAdjustHeight(serialBuffer.substring(7).toInt(), NULL);
    }
    else
    {
        Serial.print("UNKNOWN COMMAND");
    }

    Serial.println();
}

static void serialTask(void *parameter)
{
    while (1)
    {
        while (Serial && Serial.available())
        {
            char c = Serial.read();
            if (c == '\n' || c == '\r')
            {
                if (serialBuffer.length() > 0)
                {
                    serialHandleCommand();
                    serialBuffer = "";
                }
            }
            else
            {
                serialBuffer += c;
            }
        }
        delay(10);
    }
}

void serialSetup()
{
    serialBuffer.reserve(128);
    Serial.begin(115200);
    CREATE_TASK(serialTask, "serial", 2, NULL);
}
