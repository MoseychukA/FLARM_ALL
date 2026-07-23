/*
  Модуль ESP32RF.h
  Назначение:
  - Публичный интерфейс TFT-части проекта.

  Что содержит файл:
  - Константы, связанные с отображением радара и накоплением измерений.
  - Объявления функций запуска и циклического обслуживания дисплея.
*/

#pragma once

#include "Container.h"


/*BasicMAC      LoRaWAN  LoRa Alliance    C.
   LMiC  IBM,    ,     / .
      A,  B   C.*/
#define USE_BASICMAC
extern const uint8_t whitening_pattern[] PROGMEM;
#define speed_array_size                  10    //      
#define course_array_size                 10    //      
#define altitude_array_size                6    //      
#define DATA_MEASURE_THRESHOLD           500    //     
#define BUTTON_OFF_DELAY            300000ul    // Автовозврат ручного диапазона через 5 минут
#define TFT_EXPIRATION_TIME               10             /*10 seconds */



enum
{
    DISPLAY_NONE,
    DISPLAY_TFT_32,
};


enum
{
    VIEW_RSSI_OFF,
    VIEW_RSSI_ON
};


enum
{
    DISPLAY_EXTERNAL,
    DISPLAY_BUILT_IN,
    DISPLAY_OFF
};

enum
{
    INFO_DISTLAY_OFF,
    INFO_DISPLAY_COORDINATE,
    INFO_DISPLAY_MAXI,
    INFO_DISPLAY_LORA_RAW
};

enum
{
    VIEW_COORD_OFF,
    VIEW_COORD_ON
};


void Display_setup();
void Display_loop();
