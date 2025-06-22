/*
 * EEPROMHelper.h
 * Copyright (C) 2016-2023 Linar Yusupov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef EEPROMHELPER_H
#define EEPROMHELPER_H

#ifdef __cplusplus
#include "SoC.h"
#endif /* __cplusplus */

#if !defined(EXCLUDE_EEPROM)
#ifdef __cplusplus
#include <EEPROM.h>
#endif /* __cplusplus */
#endif /* EXCLUDE_EEPROM */

#define FLYRF_EEPROM_MAGIC   0xBABADEDA
#define FLYRF_EEPROM_VERSION 0x00000060

enum
{
	EEPROM_EXT_LOAD,
	EEPROM_EXT_DEFAULTS,
	EEPROM_EXT_STORE
};

typedef struct Settings {
    uint8_t  mode;
    uint8_t  rf_protocol;
    uint8_t  band;
    uint8_t  aircraft_type;
    uint8_t  txpower;
    uint8_t  volume;
    uint8_t  led_num;
    uint8_t  pointer;

    bool     nmea_g:1;
    bool     nmea_p:1;
    bool     nmea_l:1;
    bool     nmea_s:1;
    bool     resvd1:1;
    uint8_t  nmea_out:3;

    uint8_t  bluetooth:3; /* ESP32 built-in Bluetooth */
    uint8_t  alarm:3;
    bool     stealth:1;
    bool     no_track:1;

    uint8_t  d1090:3;
    uint8_t  json:2;

    uint8_t  power_save;

    int8_t   freq_corr; /* +/-, kHz */
 
     /* Use a key provided by (local) gliding contest organizer */
    uint32_t igc_key[4];

    uint8_t  gsm_send : 4;
    int16_t  alarm_attention;      /*Внимание */
    int16_t  alarm_warning;        /*Предупреждение */
    int16_t  alarm_danger;         /*Тревога */
    int16_t  alarm_height;         /*Тревога по высоте*/

    uint8_t  CountNotReadMessage;         // Счетчик количества непрочитанных сообщений
    uint8_t  CurrentCountMessage;         // Счетчик количества сообщений
    uint8_t  Message_Not_Confirmed_flag;  // сохранить флаг о получение информации с авиа приемника
    uint32_t block_addr;                                     //
    uint8_t  mail[1/*Max_Count_Block_Message*/][96]; // Массив для сообщений почты
    bool     rssi_view : 2;
    bool     ram_view : 2;
    bool     akk_view : 2;
    bool     sos_view : 2;
    int16_t akk_min;                // Минимальное значение заряда аккумулятора
    int16_t akk_max;                // Максимальное значение заряда аккумулятора
    int16_t akk_koef;               // Коэффициет пересчета заряда аккумулятора
    bool     input_coordinates : 2;
    bool     input_N_S : 2;
    bool     input_E_W : 2;
    float  test_latitude;
    float  test_longitude;
    bool   view_test_coord : 2;
    bool   coord_max100 : 2;
} __attribute__((packed)) settings_t;

typedef struct EEPROM_S {
    uint32_t  magic;
    uint32_t  version;
    settings_t settings;
} eeprom_struct_t;

typedef union EEPROM_U {
   eeprom_struct_t field;
   uint8_t raw[sizeof(eeprom_struct_t)];
} eeprom_t;

void EEPROM_setup(void);
void EEPROM_defaults(void);
void EEPROM_store(void);
extern settings_t *settings;

#endif /* EEPROMHELPER_H */
