#include <Arduino.h>

#include "ranging.h"
#include "desk.h"
#include "mqtt.h"

String serialBuffer;

void serialSetup()
{
    serialBuffer.reserve(128);
    Serial.begin(115200);
}

static void serialHandleCommand()
{
    serialBuffer.trim();
    serialBuffer.toLowerCase();

    Serial.print(serialBuffer);
    Serial.print(" ");

    if (serialBuffer.equals("range"))
    {
        mqttSendJSON({ 0 }, "range", "OK");
    }
    else if (serialBuffer.startsWith("adjust "))
    {
        deskAdjustHeight(serialBuffer.substring(7).toInt(), { 0 });
    }
    else
    {
        Serial.print("UNKNOWN COMMAND");
    }

    Serial.println();
}

void serialLoop()
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
}
