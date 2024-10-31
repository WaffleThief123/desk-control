#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "ranging.h"
#include "desk.h"
#include "serial.h"
#include "mqtt.h"
#include "util.h"
#include <WiFi.h>

static void networkWatchdog(void *parameter)
{
    while (1)
    {
        delay(1000);

        static unsigned long lastOkayTime = millis();
        if (WiFi.isConnected() && mqttIsConnected())
        {
            lastOkayTime = millis();
            continue;
        }

        if (millis() - lastOkayTime > WIFI_MQTT_TIMEOUT)
        {
            SERIAL_PORT.println("Network main timeout!");
            ESP.restart();
        }
    }
}

static void arduinoOTATask(void *parameter)
{
    while (1)
    {
        ArduinoOTA.handle();
        delay(100);
    }
}

void setup()
{
    deskSetup();

    serialSetup();
    SERIAL_PORT.println("Booting...");

    CREATE_TASK_IO(networkWatchdog, "networkWatchdog", 2, NULL);

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    WiFi.waitForConnectResult();

    SERIAL_PORT.println("WiFi initialized");

    ArduinoOTA.setHostname(WIFI_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();

    SERIAL_PORT.println("OTA initialized");

    rangingSetup();

    mqttSetup();

    CREATE_TASK_IO(arduinoOTATask, "arduinoOTA", 1, NULL);

    SERIAL_PORT.println("Boot complete");
}

void loop()
{
    vTaskDelete(NULL);
}
