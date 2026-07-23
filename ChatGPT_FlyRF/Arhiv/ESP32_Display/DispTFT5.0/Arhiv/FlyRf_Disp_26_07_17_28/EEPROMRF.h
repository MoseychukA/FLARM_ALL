/*
  Модуль EEPROMRF.h
  Назначение:
  - Публичное описание структуры настроек устройства и операций с EEPROM.

  Что содержит файл:
  - Константы формата блока настроек в EEPROM.
  - Описание структуры settings_t с параметрами системы.
  - Объявления функций загрузки, сохранения, сброса и изменения настроек.
*/

#pragma once
#include <Arduino.h>
#include <EEPROM.h>

#define FLYRF_EEPROM_MAGIC   0xBABADEDAUL
#define FLYRF_EEPROM_VERSION 0x0000007EUL
#define FLYRF_EEPROM_PREV_VERSION 0x0000007DUL

#ifndef EEPROM_SIZE
#define EEPROM_SIZE 1024U
#endif

enum
{
    EEPROM_EXT_LOAD,
    EEPROM_EXT_DEFAULTS,
    EEPROM_EXT_STORE
};

typedef struct Settings {
    uint8_t  mode;  // Параметр конфигурации: хранит выбранный режим, адрес, настройки модуля или пользовательское значение.
    uint8_t  rf_protocol;  
    uint8_t  band;  
    uint8_t  aircraft_type;  
    uint8_t  txpower;  
    uint8_t  test_mode;  // Параметр конфигурации: хранит выбранный режим, адрес, настройки модуля или пользовательское значение.

    uint8_t  nmea_p:1;
    uint8_t  nmea_out:3;

    uint8_t  bluetooth:3;
    uint32_t igc_key[4];  

    uint8_t  tracker_send:4;
    int16_t  alarm_attention;  
    int16_t  alarm_warning;  
    int16_t  alarm_danger;  
    int16_t  alarm_height;  

    uint8_t  rssi_view:2;
    uint8_t  input_coordinates:2;
    uint8_t  input_N_S:2;
    uint8_t  input_E_W:2;
    float    local_latitude;  
    float    local_longitude;  
    uint8_t  view_test_coord:2;
    uint8_t  display_sos;      // Разрешение вывода крупной надписи SOS на TFT при высоком уровне GPIO42.
    uint8_t  display_set;  
    uint8_t  lan_state_view;  
    uint8_t  tft_memory_view;  
    uint8_t  power_current_view;
    uint8_t  power_voltage_view;
    uint8_t  power_battery_view;
    uint8_t  serial_out;  // Объект интерфейса связи, через который выполняется прием, передача или обслуживание внешнего канала.
    uint8_t  rs485_out;  
    int16_t  threshold_level;  
    uint8_t  g_localIP[4];  
    uint8_t  g_gatewayIP[4];  
    uint8_t  g_subnetMask[4];  
    uint8_t  g_dns_server[4];  
    uint8_t  lora_fixed_freq;  
    uint8_t  lora_profile;  // 0=OGN compatible, 1=Long range, 2=Max range, 3=Fast robust, 4=Custom SF/BW/CR.
    uint8_t  lora_custom_sf; // Пользовательский LoRa SF: 7..12.
    uint8_t  lora_custom_bw; // Пользовательская LoRa BW: 0=125, 1=250, 2=500 kHz.
    uint8_t  lora_custom_cr; // Пользовательская LoRa CR: 0=4/5, 1=4/6, 2=4/7, 3=4/8.
    uint8_t  lora_fixed_channel;  
    uint8_t  radar_range_mode;  
    uint16_t udp_port;  
    uint32_t block_addr[3];  
    uint8_t  gps_state_view;
} __attribute__((packed)) settings_t;

typedef struct EEPROM_S {
    uint32_t   magic;  
    uint32_t   version;  
    settings_t settings;  // Структура настроек, состояния или набора рабочих данных.
} eeprom_struct_t;

typedef union EEPROM_U {
    eeprom_struct_t field;  
    uint8_t raw[sizeof(eeprom_struct_t)];
} eeprom_t;

void EEPROM_setup(void);
void EEPROM_defaults(void);
void EEPROM_store(void);
void EEPROM_clear(void);

settings_t* EEPROM_getSettings(void);
void EEPROM_setThresholdLevel(int16_t level, bool storeNow = true);

extern settings_t* settings;  // Структура настроек, состояния или набора рабочих данных.
