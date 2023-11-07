// Visual Micro is in vMicro>General>Tutorial Mode
// 
/*
    Name:       ESP32_RTL_SDR_23_11_04_03.ino
    Created:	04.11.2023 10:14:49
    Author:     MASTER\Alex
*/
#include <Arduino.h>              // define I/O functions
#include <SD.h>                   // Поддержка SD карты
#include "SPIFFS.h"
#include "FS.h"
#include <Wire.h>   

#include <usbhub.h>
#include "pgmstrings.h"

//***********************
#include "tuner_fc0012.h"
#include "tuner_fc0013.h"
#include "tuner_r82xx.h"
#include "reg_field.h"
#include "rtl-sdr_export.h"
#include "rtl-sdr.h"


void setup()
{
  Serial.begin(115200);
#if !defined(__MIPSEL__)
    while (!Serial); // Подождите, пока подключится последовательный порт — используется на Leonardo, Teensy и других платах со встроенным последовательным соединением USB CDC.
#endif
    Serial.println(__FILE__);
    Serial.println("Start");

}


void loop()
{


}
