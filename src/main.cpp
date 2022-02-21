#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "ranging.h"
#include "desk.h"

void setup() {
    deskSetup();

    Serial.begin(115200);
    while (!Serial) delay(10);
    while (Serial.readStringUntil('\n').length() <= 0) delay(10);
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
    rangingStart();
    int16_t distance = rangingGetDistance();
    rangingStop();
    Serial.print("Ranging ");
    Serial.println(distance);
}
