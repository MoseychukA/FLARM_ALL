#pragma once

#include "Container.h"


/*BasicMAC — это переносимая реализация спецификации LoRaWAN™ от LoRa™ Alliance на языке программирования C.
Это ответвление библиотеки LMiC от IBM, которое поддерживает несколько регионов, выбираемых во время компиляции и/или выполнения.
Оно может работать с устройствами класса A, класса B и класса C.*/
#define USE_BASICMAC
extern const uint8_t whitening_pattern[] PROGMEM;
#define speed_array_size                  10    // Величина массива фильтра скорости сторннего самолета
#define course_array_size                 10    // Величина массива фильтра скорости сторннего самолета
#define altitude_array_size                6    // Величина массива фильтра высоты сторннего самолета
#define DATA_MEASURE_THRESHOLD           500    // через сколько миллисекунд обновлять показания
#define BUTTON_OFF_DELAY            240000ul    // время выключения ручного диапазона просмотра через 4 минуты
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
    INFO_DISPLAY_MAXI
};

enum
{
    VIEW_COORD_OFF,
    VIEW_COORD_ON
};


void Display_setup();
void Display_loop();