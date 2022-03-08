#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HomeAssistantMQTT.h>

#include "mqtt.h"

#include "config.h"
#include "desk.h"
#include "ranging.h"
#include "util.h"

#define TAKE_ATTRIBUTE_SEMAPHORE() xSemaphoreTake(attributeSetMutex, 10 / portTICK_PERIOD_MS)
#define GIVE_ATTRIBUTE_SEMAPHORE() xSemaphoreGive(attributeSetMutex)

static WiFiClient espMqttClient;
static PubSubClient mqttClient(espMqttClient);

static bool doHeightUpdate = false;
static bool doDirectionUpdate = false;
static unsigned long lastHeightUpdateTime = 0;
static int8_t lastMovingDirection = -9;
static SemaphoreHandle_t attributeSetMutex;

static HomeAssistantMQTTDevice deskControlDevice("number", DEVICE_NAME " Control");
static HomeAssistantMQTTDevice deskStateDevice("sensor", DEVICE_NAME " State");
static HomeAssistantMQTTDevice deskStopDevice("button", DEVICE_NAME " Stop");

static void mqttFullRefresh()
{
    doHeightUpdate = true;
    doDirectionUpdate = true;
    deskControlDevice.refresh();
    deskStateDevice.refresh();
    deskStopDevice.refresh();
}

void mqttDoHeightUpdate()
{
    doHeightUpdate = true;
    doDirectionUpdate = true;
}

void mqttSetLastError(String lastError)
{
    if (TAKE_ATTRIBUTE_SEMAPHORE()) {
        deskControlDevice.setAttribute("last_error", lastError);
        GIVE_ATTRIBUTE_SEMAPHORE();
    }
    SERIAL_PORT.println("ERROR: " + lastError);
}

void mqttSetStopReason(String stopReason)
{
    if (TAKE_ATTRIBUTE_SEMAPHORE()) {
        deskControlDevice.setAttribute("stop_reason", stopReason);
        GIVE_ATTRIBUTE_SEMAPHORE();
    }
}

void mqttSetDebug(String debugMsg)
{
    if (!debugEnabled)
    {
        return;
    }
    SERIAL_PORT.println("DEBUG: " + debugMsg);        
}

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    char str[len + 1];
    memcpy(str, payload, len);
    str[len] = 0;

    if (debugEnabled)
    {
        SERIAL_PORT.print("MQTT <");
        SERIAL_PORT.print(topic);
        SERIAL_PORT.print("> ");
        SERIAL_PORT.println(str);
    }

    if (strcmp(topic, HA_STATUS_TOPIC) == 0)
    {
        if (strcmp(str, "online") == 0)
        {
            mqttFullRefresh();
        }
    }
    else if (strcmp(topic, deskControlDevice.getMQTTCommandTopic().c_str()) == 0)
    {
        const int num = atoi(str);
        if (num > 0)
        {
            deskAdjustHeight(num);
        }
    }
    else if (strcmp(topic, deskStopDevice.getMQTTCommandTopic().c_str()) == 0)
    {
        deskStop();
    }
}

bool mqttIsConnected()
{
    return mqttClient.connected();
}

bool mqttEnsureConnected()
{
    if (mqttClient.connected())
    {
        return true;
    }

    if (!WiFi.isConnected())
    {
        return false;
    }

    mqttClient.setBufferSize(HA_JSON_MAX_SIZE);
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);

    String clientId = WIFI_HOSTNAME "-" + String(WiFi.macAddress());

    if (!mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD))
    {
        SERIAL_PORT.print("MQTT connection error: ");
        SERIAL_PORT.println(mqttClient.state());
        return false;
    }

    mqttClient.subscribe(deskControlDevice.getMQTTCommandTopic().c_str());
    mqttClient.subscribe(deskStopDevice.getMQTTCommandTopic().c_str());
    mqttClient.subscribe(HA_STATUS_TOPIC);

    SERIAL_PORT.println("MQTT connected");
    mqttFullRefresh();

    return true;
}

static void mqttLoopTask(void *parameter)
{
    while (1)
    {
        delay(10);
        if (mqttEnsureConnected())
        {
            mqttClient.loop();

            const unsigned long now = millis();

            if (doHeightUpdate || now - lastHeightUpdateTime > AUTO_HEIGHT_PERIOD)
            {
                rangingAcquireBit(RANGING_BIT_MQTT);
                const ranging_result_t result = rangingWaitForAnyResult();
                if (result.valid)
                {
                    deskStateDevice.setState(String(result.value));
                }
                rangingReleaseBit(RANGING_BIT_MQTT);
                doHeightUpdate = false;
                lastHeightUpdateTime = now;
            }

            if (TAKE_ATTRIBUTE_SEMAPHORE()) {
                if (doDirectionUpdate)
                {
                    const int8_t currentMovingDirection = deskGetMovingDirection();
                    if (currentMovingDirection != lastMovingDirection) {
                        deskStateDevice.setAttribute("direction", currentMovingDirection);
                        lastMovingDirection = currentMovingDirection;
                    }
                    doDirectionUpdate = false;
                }

                deskControlDevice.loop(mqttClient);
                deskStateDevice.loop(mqttClient);
                deskStopDevice.loop(mqttClient);
                GIVE_ATTRIBUTE_SEMAPHORE();
            }
        }
    }
}

void mqttSetup()
{
    lastHeightUpdateTime = millis();
    attributeSetMutex = xSemaphoreCreateMutex();

    deskControlDevice.setConfig("max", DESK_HEIGHT_MAX);
    deskControlDevice.setConfig("min", DESK_HEIGHT_MIN);
    deskControlDevice.setConfig("step", 1);
    deskControlDevice.setConfig("unit_of_measurement", "mm");
    deskStateDevice.setConfig("unit_of_measurement", "mm");
    mqttFullRefresh();
    mqttEnsureConnected();
    CREATE_TASK_IO(mqttLoopTask, "mqttLoop", 10, NULL);
}
