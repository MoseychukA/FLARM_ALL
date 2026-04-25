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
#define FLYRF_EEPROM_VERSION 0x00000071UL

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
    uint8_t  mode;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint8_t  rf_protocol;  // Счетчик, индекс, позиция или номер элемента.
    uint8_t  band;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint8_t  aircraft_type;  // Параметр геометрии, координаты, размера или угла.
    uint8_t  txpower;  // Параметр геометрии, координаты, размера или угла.
    uint8_t  test_mode;  // Параметр конфигурации интерфейса, адресации или выбранного режима.

    uint8_t  nmea_p:1;
    uint8_t  nmea_out:3;

    uint8_t  bluetooth:3;
    uint32_t igc_key[4];  // Параметр геометрии, координаты, размера или угла.

    uint8_t  tracker_send:4;
    int16_t  alarm_attention;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    int16_t  alarm_warning;  // Параметр геометрии, координаты, размера или угла.
    int16_t  alarm_danger;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    int16_t  alarm_height;  // Параметр геометрии, координаты, размера или угла.

    uint8_t  rssi_view:2;
    uint8_t  input_coordinates:2;
    uint8_t  input_N_S:2;
    uint8_t  input_E_W:2;
    float    local_latitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    float    local_longitude;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    uint8_t  view_test_coord:2;
    uint8_t  display_set;  // Параметр геометрии, координаты, размера или угла.
    uint8_t  lan_state_view;  // Параметр геометрии, координаты, размера или угла.
    uint8_t  tft_memory_view;  // Объект внешнего интерфейса, экрана, порта или канала связи.
    uint8_t  serial_out;  // Объект внешнего интерфейса, экрана, порта или канала связи.
    uint8_t  rs485_out;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    int16_t  threshold_level;  // Параметр геометрии, координаты, размера или угла.
    uint8_t  g_localIP[4];  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint8_t  g_gatewayIP[4];  // Параметр геометрии, координаты, размера или угла.
    uint8_t  g_subnetMask[4];  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint8_t  g_dns_server[4];  // Объект внешнего интерфейса, экрана, порта или канала связи.
    uint8_t  lora_fixed_freq;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint8_t  lora_profile;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint8_t  lora_fixed_channel;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint8_t  radar_range_mode;  // Числовой параметр навигации, радиообмена, геометрии или измерения.
    uint16_t udp_port;  // Параметр конфигурации интерфейса, адресации или выбранного режима.
    uint32_t block_addr[3];  // Параметр конфигурации интерфейса, адресации или выбранного режима.
} __attribute__((packed)) settings_t;

typedef struct EEPROM_S {
    uint32_t   magic;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint32_t   version;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    settings_t settings;  // Структура настроек, состояния или набора рабочих данных.
} eeprom_struct_t;

typedef union EEPROM_U {
    eeprom_struct_t field;  // Служебная переменная, используемая для промежуточных вычислений и логики модуля.
    uint8_t raw[sizeof(eeprom_struct_t)];
} eeprom_t;

void EEPROM_setup(void);
void EEPROM_defaults(void);
void EEPROM_store(void);
void EEPROM_clear(void);

settings_t* EEPROM_getSettings(void);
void EEPROM_setThresholdLevel(int16_t level, bool storeNow = true);

extern settings_t* settings;  // Структура настроек, состояния или набора рабочих данных.
