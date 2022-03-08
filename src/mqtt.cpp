#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HAMqttDevice.h>

#include "mqtt.h"

#include "config.h"
#include "desk.h"
#include "ranging.h"
#include "util.h"

#define HA_STATUS_TOPIC "homeassistant/status"

static WiFiClient espMqttClient;
static PubSubClient mqttClient(espMqttClient);

static bool doHeightUpdate = false;
static bool doAttributeUpdate = false;
static bool doConfigUpdate = false;

static HAMqttDevice deskControlDevice(DEVICE_NAME " Control", HAMqttDevice::NUMBER, "homeassistant");
static HAMqttDevice deskStateDevice(DEVICE_NAME " State", HAMqttDevice::SENSOR, "homeassistant");
static HAMqttDevice deskStopDevice(DEVICE_NAME " Stop", HAMqttDevice::BUTTON, "homeassistant");

static String lastError;
static String stopReason;

static unsigned long lastHeightUpdateTime;

void mqttDoHeightUpdate()
{
    doHeightUpdate = true;
}

void mqttDoAttributeUpdate()
{
    doAttributeUpdate = true;
}

void mqttSetLastError(String _lastError)
{
    lastError = _lastError;
    doAttributeUpdate = true;
    SERIAL_PORT.println("ERROR: " + lastError);
}

void mqttSetStopReason(String _stopReason)
{
    stopReason = _stopReason;
    doAttributeUpdate = true;
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
            doConfigUpdate = true;
            doAttributeUpdate = true;
            doHeightUpdate = true;
        }
    }
    else if (strcmp(topic, deskControlDevice.getCommandTopic().c_str()) == 0)
    {
        const int num = atoi(str);
        if (num > 0)
        {
            deskAdjustHeight(num);
        }
    }
    else if (strcmp(topic, deskStopDevice.getCommandTopic().c_str()) == 0)
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

    mqttClient.setBufferSize(4096);
    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);

    String clientId = WIFI_HOSTNAME "-" + String(WiFi.macAddress());

    if (!mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD))
    {
        SERIAL_PORT.print("MQTT connection error: ");
        SERIAL_PORT.println(mqttClient.state());
        return false;
    }

    mqttClient.subscribe(deskControlDevice.getCommandTopic().c_str());
    mqttClient.subscribe(deskStopDevice.getCommandTopic().c_str());
    mqttClient.subscribe(HA_STATUS_TOPIC);

    SERIAL_PORT.println("MQTT connected");
    doConfigUpdate = true;
    doAttributeUpdate = true;
    doHeightUpdate = true;

    return true;
}

static void mqttLoopTask(void *parameter)
{
    while (1)
    {
        if (mqttEnsureConnected())
        {
            mqttClient.loop();

            const unsigned long now = millis();

            if (now - lastHeightUpdateTime > AUTO_HEIGHT_PERIOD)
            {
                doHeightUpdate = true;
            }

            if (doConfigUpdate)
            {
                mqttClient.publish(deskControlDevice.getConfigTopic().c_str(), deskControlDevice.getConfigPayload().c_str());
                mqttClient.publish(deskStateDevice.getConfigTopic().c_str(), deskStateDevice.getConfigPayload().c_str());
                mqttClient.publish(deskStopDevice.getConfigTopic().c_str(), deskStopDevice.getConfigPayload().c_str());
                doConfigUpdate = false;
            }
            if (doAttributeUpdate)
            {
                deskControlDevice.addAttribute("last_error", lastError);
                deskControlDevice.addAttribute("stop_reason", stopReason);
                mqttClient.publish(deskControlDevice.getAttributesTopic().c_str(), deskControlDevice.getAttributesPayload().c_str());
                deskControlDevice.clearAttributes();
                deskStateDevice.addAttribute("direction", String(deskGetMovingDirection()));
                mqttClient.publish(deskStateDevice.getAttributesTopic().c_str(), deskStateDevice.getAttributesPayload().c_str());
                deskStateDevice.clearAttributes();
                doAttributeUpdate = false;
            }
            if (doHeightUpdate)
            {
                rangingAcquireBit(RANGING_BIT_MQTT);
                const ranging_result_t result = rangingWaitForAnyResult();
                if (result.valid)
                {
                    mqttClient.publish(deskStateDevice.getStateTopic().c_str(), String(result.value).c_str());
                }
                rangingReleaseBit(RANGING_BIT_MQTT);
                doHeightUpdate = false;
                lastHeightUpdateTime = now;
            }
        }
        delay(10);
    }
}

void mqttSetup()
{
    lastHeightUpdateTime = millis();
    deskControlDevice.enableAttributesTopic();
    deskStateDevice.enableAttributesTopic();
    deskControlDevice.addConfigVar("max", STRINGIFY(DESK_HEIGHT_MAX));
    deskControlDevice.addConfigVar("min", STRINGIFY(DESK_HEIGHT_MIN));
    deskControlDevice.addConfigVar("step", "1");
    mqttEnsureConnected();
    CREATE_TASK_IO(mqttLoopTask, "mqttLoop", 10, NULL);
}
