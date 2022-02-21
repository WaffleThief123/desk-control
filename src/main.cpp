#include <Arduino.h>
#include <ArduinoOTA.h>

#include "config.h"
#include "ranging.h"
#include "desk.h"
#include "buttons.h"

void setup() {
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
    rangingStart();
    int16_t distance = rangingGetDistance();
    rangingStop();
    Serial.print("Ranging ");
    Serial.println(distance);
}
