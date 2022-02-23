#include "mqtt.h"

#include "config.h"
#include "desk.h"
#include "ranging.h"

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <ArduinoJson.h>

WiFiClient espMqttClient;
PubSubClient mqttClient(espMqttClient);

void mqttCallback(char *topic, byte *payload, unsigned int len) {
    char str[len + 1];
    memcpy(str, payload, len);
    str[len] = 0;

    DynamicJsonDocument doc(256);
    deserializeJson(doc, str);

    const char *id = doc["id"].as<const char*>();

    const char *cmd = doc["command"].as<const char*>();
    if (strcmp(cmd, "adjust") == 0) {
        int16_t target = doc["target"].as<int16_t>();
        deskAdjustHeight(target, id);
    } else if (strcmp(cmd, "stop") == 0) {
        deskStop();
    } else if (strcmp(cmd, "range") == 0) {
        mqttSendJSON(id, "range", "OK");
    } else {
        mqttSendJSON(id, "error", "UNKNOWN COMMAND");
    }
}

bool mqttEnsureConnected() {
    if (mqttClient.connected()) {
        return true;
    }

    if (!WiFi.isConnected()) {
        return false;
    }

    if (deskIsMoving()) {
        return false;
    }

    mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    String clientId = WIFI_HOSTNAME "-" + String(WiFi.macAddress());

    if (!mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
        Serial.print("MQTT connection error: ");
        Serial.println(mqttClient.state());
        return false;
    }

    Serial.println("MQTT connected");

    mqttClient.subscribe(MQTT_TOPIC_SUB);
    return true;
}

void mqttSetup() {
    mqttEnsureConnected();
}

void mqttLoop() {
    if (mqttEnsureConnected()) {
        mqttClient.loop();
    }
}

void mqttSend(const char* data) {
    if (!mqttEnsureConnected()) {
        return;
    }
    mqttClient.publish(MQTT_TOPIC_PUB, data);
}

void mqttSendJSON(const char* mqttId, const char* type, const char* data, int16_t range) {
    if (range == -999) {
        range = rangingWaitAndGetDistance();
    }
    const int8_t moving = deskIsMoving();
    int16_t target = deskGetTarget();
    if (target <= 0) {
        target = range;
    }

    Serial.print("<");
    Serial.print(type);
    Serial.print("> ");
    Serial.print(data);
    Serial.print(" [");
    Serial.print(range);
    Serial.print(" => ");
    Serial.print(target);
    Serial.print(" @ ");
    Serial.print(moving);
    Serial.println("]");

    char buf[256];

    StaticJsonDocument<256> doc;
    if (mqttId && mqttId[0]) {
        doc["id"] = mqttId;
    }
    doc["type"] = type;
    doc["data"] = data;
    doc["range"] = range;
    doc["moving"] = moving;
    doc["target"] = target;
    int len = serializeJson(doc, buf);

    buf[len] = 0;

    mqttSend(buf);
}
