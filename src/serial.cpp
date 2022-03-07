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

    SERIAL_PORT.print(serialBuffer);
    SERIAL_PORT.print(" ");

    if (serialBuffer.equals("range"))
    {
        rangingAcquireBit(RANGING_BIT_SERIAL);
        const ranging_result_t rangingResult = rangingWaitForNextResult();
        if (rangingResult.valid)
        {
            SERIAL_PORT.print(rangingResult.value);
            SERIAL_PORT.print(" @ ");
            SERIAL_PORT.print(rangingResult.time);
        }
        else
        {
            SERIAL_PORT.print("ERROR");
        }
        rangingReleaseBit(RANGING_BIT_SERIAL);
    }
    else if (serialBuffer.equals("debug"))
    {
        debugEnabled = !debugEnabled;
        SERIAL_PORT.print(debugEnabled);
    }
    else if (serialBuffer.equals("restart"))
    {
        if (!doRestart(false))
        {
            SERIAL_PORT.print("NOT ALLOWED");
        }
    }
    else if (serialBuffer.equals("restart force"))
    {
        if (!doRestart(true))
        {
            SERIAL_PORT.print("NOT ALLOWED");
        }
    }
    else if (serialBuffer.startsWith("adjust "))
    {
        deskAdjustHeight(serialBuffer.substring(7).toInt());
    }
    else
    {
        SERIAL_PORT.print("UNKNOWN COMMAND");
    }

    SERIAL_PORT.println();
}

static void serialTask(void *parameter)
{
    while (1)
    {
        while (SERIAL_PORT && SERIAL_PORT.available())
        {
            char c = SERIAL_PORT.read();
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
    SERIAL_PORT.begin(115200);
    CREATE_TASK_IO(serialTask, "serial", 5, NULL);
}
