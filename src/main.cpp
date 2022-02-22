#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "ranging.h"
#include "desk.h"
#include "buttons.h"

void setup() {
    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    deskSetup();
    buttonsSetup();

    Serial.begin(115200);
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

    Serial.println("Boot complete");
}

void loop() {
    String str = Serial.readStringUntil('\n');
    if (str.length() <= 0) {
        delay(100);
        return;
    }
    str.trim();
    str.toLowerCase();

    if (str.equals("range")) {
        rangingStart();
        while (!Serial.available()) {
            const int16_t distance = rangingGetDistance();
            if (distance >= 0) {
                Serial.println(distance);
            }
            delay(1);
        }
        rangingStop();
        return;
    }

    int16_t targetHeight = str.toInt();
    if (targetHeight > 100 && targetHeight < 1500) {
        deskAdjustHeight(targetHeight);
    }
}
