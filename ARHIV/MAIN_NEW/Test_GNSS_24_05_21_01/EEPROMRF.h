
#ifndef EEPROMHELPER_H
#define EEPROMHELPER_H

#ifdef __cplusplus
#include "SoC.h"
#endif /* __cplusplus */


#ifdef __cplusplus
#include <EEPROM.h>
#endif /* __cplusplus */
#include "Configuration_ESP32.h"

#define SOFTRF_EEPROM_MAGIC   0xBABADEDA
#define SOFTRF_EEPROM_VERSION 0x00000060

enum
{
	EEPROM_EXT_LOAD,
	EEPROM_EXT_DEFAULTS,
	EEPROM_EXT_STORE
};

typedef struct Settings {
 /*   uint8_t  mode;
    uint8_t  rf_protocol;
    uint8_t  band;
    uint8_t  aircraft_type;
    uint8_t  txpower;*/
    uint8_t  volume;
    uint8_t  led_num;
    uint8_t  pointer;

    bool     nmea_g:1;
    bool     nmea_p:1;
    bool     nmea_l:1;
    bool     nmea_s:1;
    bool     resvd1:1;
    uint8_t  nmea_out:3;

    //uint8_t  bluetooth:3; /* ESP32 built-in Bluetooth */
    //uint8_t  alarm:3;
    //bool     stealth:1;
    //bool     no_track:1;

    //uint8_t  gdl90:3;
    //uint8_t  d1090:3;
    uint8_t  json:2;

    uint8_t  power_save;
    //int16_t  alarm_attention;      /*Внимание */
    //int16_t  alarm_warning;        /*Предупреждение */
    //int16_t  alarm_danger;         /*Тревога */
    //int16_t  alarm_height;         /*Тревога по высоте*/

    //int8_t   freq_corr;            /* +/-, kHz */
    //uint8_t  CountNotReadMessage;         // Счетчик количества непрочитанных сообщений
    //uint8_t  CurrentCountMessage;         // Счетчик количества сообщений
    //uint8_t  Message_Not_Confirmed_flag;  //сохранить флаг о получение информации с авиа приемника

    uint8_t  units : 2;

    ///* Используйте ключ, предоставленный (местным) организатором соревнований по планерному спорту. */
    //uint32_t igc_key[4];

    //uint8_t  mail[Max_Count_Block_Message][128]; // Массив для сообщений почты

} __attribute__((packed)) settings_t;

typedef struct EEPROM_S 
{
    uint32_t  magic;
    uint32_t  version;
    settings_t settings;
} eeprom_struct_t;

typedef union EEPROM_U 
{
   eeprom_struct_t field;
   uint8_t raw[sizeof(eeprom_struct_t)];
} eeprom_t;

void EEPROM_setup(void);
void EEPROM_defaults(void);
void EEPROM_store(void);
extern settings_t *settings;

#endif /* EEPROMHELPER_H */
