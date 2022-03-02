#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

#include "mqtt.h"

#include "config.h"
#include "desk.h"
#include "ranging.h"
#include "util.h"

static WiFiClient espMqttClient;
static PubSubClient mqttClient(espMqttClient);

static String lastErrorCode = "";
void mqttSetLastError(String errorCode)
{
    lastErrorCode = errorCode;
    SERIAL_PORT.println("ERROR: " + errorCode);
}

void mqttCallback(char *topic, byte *payload, unsigned int len)
{
    char str[len + 1];
    memcpy(str, payload, len);
    str[len] = 0;

    SERIAL_PORT.print("MQTT command: ");
    SERIAL_PORT.println(str);

    DynamicJsonDocument doc(256);
    DeserializationError err = deserializeJson(doc, str);
    if (err != DeserializationError::Ok)
    {
        SERIAL_PORT.print("MQTT JSON error: ");
        SERIAL_PORT.println(err.c_str());
        return;
    }

    const char *id = doc["id"].as<const char *>();

    const char *cmd = doc["command"].as<const char *>();
    if (strcmp(cmd, "adjust") == 0)
    {
        int16_t target = doc["target"].as<int16_t>();
        deskAdjustHeight(target, id);
    }
    else if (strcmp(cmd, "stop") == 0)
    {
        deskStop();
    }
    else if (strcmp(cmd, "range") == 0)
    {
        mqttSendJSON(id, "range", "OK");
    }
    else if (strcmp(cmd, "restart") == 0)
    {
        bool force = doc["force"].as<bool>();
        if (!doRestart(force))
        {
            mqttSendJSON(id, "status", "RESTART NOT ALLOWED");
        }
    }
    else
    {
        mqttSendJSON(id, "error", "UNKNOWN COMMAND");
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

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);

    String clientId = WIFI_HOSTNAME "-" + String(WiFi.macAddress());

    if (!mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD))
    {
        SERIAL_PORT.print("MQTT connection error: ");
        SERIAL_PORT.println(mqttClient.state());
        return false;
    }

    SERIAL_PORT.println("MQTT connected");

    mqttClient.subscribe(MQTT_TOPIC_SUB);

    mqttSendJSON(NULL, "status", "Connected");
    return true;
}

static void mqttLoopTask(void *parameter)
{
    while (1)
    {
        if (mqttEnsureConnected())
        {
            mqttClient.loop();
            if (!lastErrorCode.isEmpty())
            {
                mqttSendJSON(NULL, "error", lastErrorCode.c_str(), -1);
                lastErrorCode = "";
            }
        }
        delay(10);
    }
}

void mqttSetup()
{
    mqttEnsureConnected();
    CREATE_TASK_IO(mqttLoopTask, "mqttLoop", 1, NULL);
}

void mqttSend(const char *data)
{
    if (!mqttEnsureConnected())
    {
        return;
    }
    mqttClient.publish(MQTT_TOPIC_PUB, data);
}

void mqttSendJSON(const char *mqttId, const char *type, const char *data, int16_t range)
{
    if (range == -999)
    {
        const ranging_result_t rangingResult = rangingWaitForAnyResult();
        if (rangingResult.valid)
        {
            range = rangingResult.value;
        }
        else
        {
            range = -1;
        }
    }
    const int8_t movingDirection = deskGetMovingDirection();
    const int16_t target = deskGetTarget();

    char buf[256];

    StaticJsonDocument<256> doc;
    if (mqttId && mqttId[0])
    {
        doc["id"] = mqttId;
    }
    doc["type"] = type;
    doc["data"] = data;
    doc["range"] = range;
    doc["direction"] = movingDirection;
    doc["target"] = target;
    int len = serializeJson(doc, buf);

    buf[len] = 0;

    if (debugEnabled)
    {
        SERIAL_PORT.println(buf);
    }

    mqttSend(buf);
}
