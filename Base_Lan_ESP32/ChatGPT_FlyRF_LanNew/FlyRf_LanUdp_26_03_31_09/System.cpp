#include <Arduino.h>
#include <cstring>
#include "System.h"
#include "TrafficDB.h"
#include "ESP32RF.h"
#include "LANRF.h"
#include "RF.h"
#include "Log.h"
#include "NMEA.h"
#include "RP2040Bridge.h"
#include "EEPROMRF.h"
#include "WiFiRF.h"
#include "WebRF.h"
#include "DeviceInfo.h"
#include "OTA.h"
#include "RS485Display.h"

void SystemSetup()
{
    Serial.println();
    Serial.println(F("Start setup"));

    Log_setup();
    Serial.println(F("[SETUP] Log ready"));

    TrafficDB.init();
    Serial.println(F("[SETUP] Container ready"));

    EEPROM_setup();
    Serial.println(F("[SETUP] EEPROM ready"));

    if (settings != nullptr)
    {
        ThisAircraft.addr = getChipId() & 0x00FFFFFFUL;
        ThisAircraft.squawk = 1111;
        memset(ThisAircraft.callsign, ' ', sizeof(ThisAircraft.callsign));
        memcpy(ThisAircraft.callsign, "FlyRF", 5);
        ThisAircraft.altitude = 0.0f;
        ThisAircraft.pressure_altitude = 0.0f;
        ThisAircraft.speed = 0.0f;
        ThisAircraft.course = 0.0f;
        ThisAircraft.vert_rate = 0;
        ThisAircraft.latitude = settings->local_latitude;
        ThisAircraft.longitude = settings->local_longitude;
        ThisAircraft.local_latitude = settings->local_latitude;
        ThisAircraft.local_longitude = settings->local_longitude;
        ThisAircraft.rp2040_gain = settings->threshold_level;
    }
    Serial.printf("[SETUP] Local coordinates: %.5f, %.5f\r\n", ThisAircraft.local_latitude, ThisAircraft.local_longitude);
    Serial.printf("[SETUP] RP2040 gain: %d\r\n", (int)ThisAircraft.rp2040_gain);

    Display_setup();
    Serial.println(F("[SETUP] Display ready"));

    const byte rfType = RF_setup();
    if (rfType != RF_IC_NONE)
    {
        Serial.println(F("[SETUP] LoRa setup done"));
    }
    else
    {
        Serial.println(F("[SETUP] LoRa module not detected"));
    }

    RP2040Bridge_setup();
    Serial.println(F("[SETUP] RP2040 setup done"));

    WiFi_setup();
    Serial.println(F("[SETUP] WiFi setup done"));

    Web_setup();
    Serial.println(F("[SETUP] Web setup done"));

    LAN_setup();
    Serial.println(F("[SETUP] LAN setup done"));

    RS485Display_setup();
    Serial.println(F("[SETUP] RS485 setup done"));

    NMEA_setup();
    Serial.println(F("[SETUP] NMEA setup done"));

    OTA_setup();
    Serial.println(F("[SETUP] OTA setup done"));

    Serial.println(F("Setup End"));
}

void SystemLoop()
{
    TrafficDB.removeStale();
    RF_loop();
    ParseData();
    RP2040Bridge_loop();
    WiFi_loop();
    Web_loop();
    LAN_loop();
    RS485Display_loop();
    NMEA_loop();
    OTA_loop();
    Display_loop();
}
