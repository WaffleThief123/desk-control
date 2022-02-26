#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "ranging.h"
#include "desk.h"
#include "buttons.h"
#include "serial.h"
#include "mqtt.h"
#include "util.h"

#ifndef USE_ESP_IDF_GPIO
#error "Need USE_ESP_IDF_GPIO"
#endif

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
            Serial.println("Network main timeout!");

            if (doRestart(false))
            {
                vTaskDelete(NULL);
                break;
            }
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
    buttonsSetup();

    serialSetup();
    Serial.println("Booting...");

    WiFi.mode(WIFI_STA);
    WiFi.setHostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.println("WiFi initialized");

    ArduinoOTA.setHostname(WIFI_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();

    Serial.println("OTA initialized");

    rangingSetup();

    mqttSetup();

    CREATE_TASK_IO(networkWatchdog, "networkWatchdog", 2, NULL);
    CREATE_TASK_IO(arduinoOTATask, "arduinoOTA", 1, NULL);

    Serial.println("Boot complete");

    mqttSendJSON(NULL, "status", "BOOT OK", -1);
}

void loop()
{
    delay(1000);
}