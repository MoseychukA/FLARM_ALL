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
#if defined(ENERGIA_ARCH_CC13XX) || defined(ENERGIA_ARCH_CC13X2)
#include <EEPROM_CC13XX.h>
#elif defined(ARDUINO_ARCH_SAMD)
#include <FlashAsEEPROM.h>
#else
#ifdef __cplusplus
#include <EEPROM.h>
#endif /* __cplusplus */
#endif /* CC13XX or CC13X2 */
#endif /* EXCLUDE_EEPROM */

#define SOFTRF_EEPROM_MAGIC   0xBABADEDA
#define SOFTRF_EEPROM_VERSION 0x00000060

enum
{
	EEPROM_EXT_LOAD,
	EEPROM_EXT_DEFAULTS,
	EEPROM_EXT_STORE
};

//typedef struct SkyView_Settings {
//    uint8_t  adapter;
//
//    uint8_t  connection : 4;
//    uint8_t  units : 2;
//    uint8_t  zoom : 2;
//
//    uint8_t  protocol;
//    uint8_t  baudrate;
//    char     ssid[18];
//    char     psk[18];
//
//    uint8_t  rotate : 2;
//    uint8_t  orientation : 1;
//    uint8_t  adb : 3;
//    uint8_t  idpref : 2;
//
//    uint8_t  data_dest : 4;
//    uint8_t  bluetooth : 4; /* ESP32 built-in Bluetooth */
//
//    char     bt_name[18];
//    char     bt_key[18];
//
//    uint8_t  vmode : 3;
//    uint8_t  voice : 2;
//    uint8_t  aghost : 3;
//
//    uint8_t  filter : 4;
//    uint8_t  power_save : 4;
//
//    uint32_t team;
//
//    uint8_t  resvd12;
//    uint8_t  resvd3;
//    uint8_t  resvd4;
//    uint8_t  resvd5;
//    uint8_t  resvd6;
//    uint8_t  resvd7;
//    uint8_t  resvd8;
//    uint8_t  resvd9;
//} __attribute__((packed)) skyview_settings_t;
//


typedef struct Settings {
    uint8_t  mode;
    uint8_t  rf_protocol;
    uint8_t  band;
    uint8_t  aircraft_type;
    uint8_t  txpower;
    uint8_t  volume;
    uint8_t  led_num;
    uint8_t  pointer;

    bool     nmea_g : 1;
    bool     nmea_p : 1;
    bool     nmea_l : 1;
    bool     nmea_s : 1;
    bool     resvd1 : 1;
    uint8_t  nmea_out : 3;

    uint8_t  bluetooth : 3; /* ESP32 built-in Bluetooth */
    uint8_t  alarm : 3;
    bool     stealth : 1;
    bool     no_track : 1;

    uint8_t  gdl90 : 3;
    uint8_t  d1090 : 3;
    uint8_t  json : 2;

    uint8_t  power_save;
    int16_t   alarm_attention;  /*Внимание */
    int16_t   alarm_warning;    /*Предупреждение */
    int16_t   alarm_danger;     /*Тревога */
    int16_t   alarm_height;     /*Тревога по высоте*/

    int8_t   freq_corr; /* +/-, kHz */

//=============================== 
    uint8_t  units : 2;

    //=============================
    uint8_t  resvd2;
    uint8_t  resvd3;
    uint8_t  resvd4;
    //=============================
    /* Use a key provided by (local) gliding contest organizer */
    uint32_t igc_key[4];
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
