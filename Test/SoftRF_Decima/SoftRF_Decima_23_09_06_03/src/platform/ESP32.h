#pragma once
#include <Arduino.h>

#ifndef PLATFORM_ESP32_H
#define PLATFORM_ESP32_H

#include <SPI.h>
#include <Wire.h>
#include "sdkconfig.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <WiFiClient.h>




class ESP32Class
{
public:
    ESP32Class();

    void ESP32_setup();
    void ESP32_post_init();
    void ESP32_loop();
    void ESP32_fini();
    void ESP32_reset();
    uint32_t ESP32_getChipId();
    void ESP32_getResetInfoPtr();
    void ESP32_getResetInfo();
    void ESP32_getResetReason();
    void ESP32_getFreeHeap();
    void ESP32_random();
    void ESP32_maxSketchSpace();
    void ESP32_WiFi_set_param();
    void ESP32_WiFi_transmit_UDP();
    void ESP32_WiFiUDP_stopAll();
    void ESP32_WiFi_hostname();
    void ESP32_WiFi_clients_count();
    void ESP32_EEPROM_begin();
    void ESP32_EEPROM_extension();
    void ESP32_SPI_begin();
    void ESP32_swSer_begin();
    void ESP32_swSer_enableRx();

    void ESP32_Display_setup();
    void ESP32_Display_loop();
    void ESP32_Display_fini();
    void ESP32_Battery_setup();
    void ESP32_Battery_param();
    void ESP32_GNSS_PPS_Interrupt_handler();
    void ESP32_get_PPS_TimeMarker();

 
    void ESP32_WDT_setup();
    void ESP32_WDT_fini();

    void ESP32_Button_setup();
    void ESP32_Button_loop();
    void ESP32_Button_fini();

private:

    uint32_t DevID_Mapper(uint32_t id);

    bool isOn;
};
//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern ESP32Class Esp32;



#endif /* PLATFORM_ESP32_H */


