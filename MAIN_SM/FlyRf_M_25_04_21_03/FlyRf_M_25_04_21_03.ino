/*
    Name:       FlyRf_M_25_04_21_01.ino
    Created:	21.04.2025 17:38:34
    Author:     Aleksandr Moseychuk
    Aleksandr.Moseychuk@decima.ru

    ESP32S3 Dev Module
*/

#include <stdio.h>       
#include <Arduino.h>    
#include "SPI.h"
#include <esp_task_wdt.h>
#include <iostream>
#include <locale.h>

#include "SoC.h"
#include "WiFiRF.h"
#include <TimeLib.h>
#include "EEPROMRF.h"
#include "OTA.h"
#include "ESP32RF.h"


ufo_t ThisAircraft;

//hardware_info_t hw_info = {
//  .model = DEFAULT_FLYRF_MODEL,
//  .revision = 0,
//  .soc = SOC_NONE,
//  .rf = RF_IC_NONE,
//  .gnss = GNSS_MODULE_NONE,
//  .baro = BARO_MODULE_NONE,
//  .display = DISPLAY_NONE,
//};


void setup()
{

    //Serial.begin(115200);
    //delay(500);

   // rst_info* resetInfo;


    Serial.println("Start setup");
    Serial.println();
    Serial.print(F(FLYRF_IDENT "-"));
    Serial.print(SoC->name);
    Serial.print(F(" FW.REV: " FLYRF_FIRMWARE_VERSION " DEV.ID: "));
    Serial.println(String(SoC->getChipId(), HEX));

    String ver_soft = __FILE__;
    int val_srt = ver_soft.lastIndexOf('\\');
    ver_soft.remove(0, val_srt + 1);
    val_srt = ver_soft.lastIndexOf('.');
    ver_soft.remove(val_srt);
    Serial.println(ver_soft);
   // service.saveVer(ver_soft);  // Сохранить строку с текущей версией.


    Serial.println("End setup");

}

void loop()
{


}
